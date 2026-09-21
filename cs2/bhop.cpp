#include "bhop.h"
#include "menu_input.h"
#include "memory.h"
#include "offsets.h"
#include "walkbot/walkbot.h"
#include "../Backend/Config/Config.h"
#include "../Frontend/Menu/Menu.h"
#include "../nerv/main.hpp"
#include "../nerv/valve/interfaces/vtables/i_csgo_input.hpp"
#include "../kiero/minhook/include/MinHook.h"

namespace bhop {
    namespace {
        constexpr int kCreateMoveIndex = 5;

        enum MoveType_t : int {
            MOVETYPE_NOCLIP = 8,
            MOVETYPE_LADDER = 9,
            MOVETYPE_OBSERVER = 10,
        };

        struct subtick_rep_t {
            int allocated_size;
            CSubtickMoveStep** elements;
        };

        struct subtick_field_t {
            void* arena;
            int current_size;
            int total_size;
            subtick_rep_t* rep;

            CSubtickMoveStep* TryAdd() {
                if (!arena)
                    return nullptr;

                if (rep && current_size < rep->allocated_size)
                    return rep->elements[current_size++];

                using create_fn_t = CSubtickMoveStep*(__fastcall*)(void*);
                using add_fn_t = CSubtickMoveStep*(__fastcall*)(subtick_field_t*, CSubtickMoveStep*);

                static create_fn_t create_fn = reinterpret_cast<create_fn_t>(
                    g_opcodes->scan_absolute(g_modules->m_modules.client_dll.get_name(),
                        offsets::signatures::create_subtick_move_step, 0x1));
                static add_fn_t add_fn = reinterpret_cast<add_fn_t>(
                    g_opcodes->scan_absolute(g_modules->m_modules.client_dll.get_name(),
                        offsets::signatures::add_subtick_to_rep_field, 0x1));

                if (!create_fn || !add_fn)
                    return nullptr;

                CSubtickMoveStep* step = create_fn(arena);
                if (!step)
                    return nullptr;

                return add_fn(this, step);
            }
        };

        subtick_field_t* GetSubtickField(CBaseUserCmdPB* base_cmd) {
            if (!base_cmd)
                return nullptr;
            return reinterpret_cast<subtick_field_t*>(reinterpret_cast<uint8_t*>(base_cmd) + 0x18);
        }

        using create_move_fn = bool(__fastcall*)(i_csgo_input*, int, std::uint8_t);
        create_move_fn g_original_create_move = nullptr;
        bool g_hooked = false;
        bool g_bWasJumping = false;

        void ProcessJump(c_user_cmd* cmd, uintptr_t localPawn) {
            if (!cmd || !localPawn)
                return;

            const int move_type = memory::Read<int>(localPawn + schema::C_BaseEntity::m_MoveType);
            const bool can_jump = move_type != MOVETYPE_LADDER
                && move_type != MOVETYPE_NOCLIP
                && move_type != MOVETYPE_OBSERVER;
            if (!can_jump)
                return;

            CBaseUserCmdPB* base_cmd = cmd->get_base_cmd();
            if (!base_cmd)
                return;

            auto& buttons = cmd->m_button_state;

            if (!(buttons.m_button_state & IN_JUMP)) {
                g_bWasJumping = false;
                return;
            }

            buttons.m_button_state &= ~static_cast<uint64_t>(IN_JUMP);
            buttons.m_button_state2 &= ~static_cast<uint64_t>(IN_JUMP);
            buttons.m_button_state3 &= ~static_cast<uint64_t>(IN_JUMP);

            const uint32_t flags = memory::Read<uint32_t>(localPawn + schema::C_BaseEntity::m_fFlags);
            const bool on_ground = (flags & FL_ONGROUND) != 0;

            if (on_ground) {
                buttons.m_button_state |= IN_JUMP;
                buttons.m_button_state3 |= IN_JUMP;
            }

            subtick_field_t* field = GetSubtickField(base_cmd);
            if (!field || !field->rep || field->total_size < 1)
                return;

            if (on_ground)
                g_bWasJumping = false;

            if (g_bWasJumping == on_ground)
                return;

            g_bWasJumping = on_ground;

            if (field->current_size != 0)
                return;

            CSubtickMoveStep* subtick = field->TryAdd();
            if (!subtick)
                subtick = base_cmd->add_subtick_moves();
            if (subtick) {
                subtick->set_button(IN_JUMP);
                subtick->set_pressed(on_ground);
                subtick->set_when(0.999f);
            }
        }

        bool __fastcall hk_create_move(i_csgo_input* input, int slot, std::uint8_t active) {
            const bool menuOpen = CMenu::get()->IsMenuOpened();

            if (!menuOpen && input)
                menu_input::RestoreWhenMenuClosed(input);
            else if (menuOpen && input)
                menu_input::PrepareBeforeCreateMove(input);

            const bool result = g_original_create_move ? g_original_create_move(input, slot, active) : true;

            if (menuOpen && input) {
                menu_input::CleanupAfterCreateMove(input);
                return result;
            }

            CConfig* cfg = CConfig::get();
            const bool walkbotActive = cfg->b["misc_walkbot"] && cfg->IsBindActive("misc_walkbot_key");
            const bool bhopActive = cfg->b["misc_bhop"];

            if ((!walkbotActive && !bhopActive) || !input)
                return result;

            if (!g_interfaces || !g_interfaces->m_engine_client || !g_interfaces->m_engine_client->is_in_game())
                return result;

            void* controller = g_ctx->m_local_controller;
            if (!controller)
                controller = g_interfaces->m_entity_system ? g_interfaces->m_entity_system->get_local_controller() : nullptr;
            if (!controller)
                return result;

            c_user_cmd* cmd = input->get_user_cmd(controller);
            if (!cmd)
                return result;

            uintptr_t localPawn = reinterpret_cast<uintptr_t>(g_ctx->m_local_pawn);
            if (!localPawn && g_interfaces->m_entity_system)
                localPawn = reinterpret_cast<uintptr_t>(g_interfaces->m_entity_system->get_local_pawn());
            if (!localPawn)
                return result;

            if (memory::Read<int>(localPawn + schema::C_BaseEntity::m_iHealth) <= 0)
                return result;

            if (walkbotActive)
                walkbot::Process(cmd, input, localPawn);
            else if (bhopActive)
                ProcessJump(cmd, localPawn);

            return result;
        }
    }

    bool InitializeHook() {
        if (g_hooked)
            return true;

        if (!g_interfaces || !g_interfaces->m_csgo_input)
            return false;

        void* create_move = vmt::get_v_method(g_interfaces->m_csgo_input, kCreateMoveIndex);
        if (!create_move)
            return false;

        if (MH_CreateHook(create_move, &hk_create_move, reinterpret_cast<void**>(&g_original_create_move)) != MH_OK)
            return false;

        if (MH_EnableHook(create_move) != MH_OK)
            return false;

        g_hooked = true;
        return true;
    }

    void Run() {
        InitializeHook();
    }
}
