#include "menu_input.h"
#include "../Frontend/Menu/Menu.h"
#include "memory.h"
#include "offsets.h"
#include "../nerv/main.hpp"
#include "../nerv/valve/interfaces/vtables/i_csgo_input.hpp"

namespace menu_input {
    namespace {
        constexpr uint64_t kBlockedButtons =
            IN_ATTACK | IN_ATTACK2 | IN_ZOOM | IN_MIDDLE_ATTACK | IN_RELOAD;

        void ClearCmdButtons(c_user_cmd* cmd) {
            if (!cmd)
                return;

            auto& buttons = cmd->m_button_state;
            buttons.m_button_state &= ~kBlockedButtons;
            buttons.m_button_state2 &= ~kBlockedButtons;
            buttons.m_button_state3 &= ~kBlockedButtons;
        }

        void StripInputButtons(uintptr_t inputAddr) {
            const uint64_t pressed = memory::Read<uint64_t>(inputAddr + offsets::csgo_input::button_pressed);
            const uint64_t mousePressed = memory::Read<uint64_t>(inputAddr + offsets::csgo_input::mouse_button_pressed);
            memory::Write<uint64_t>(inputAddr + offsets::csgo_input::button_pressed, pressed & ~kBlockedButtons);
            memory::Write<uint64_t>(inputAddr + offsets::csgo_input::mouse_button_pressed, mousePressed & ~kBlockedButtons);
        }
    }

    void PrepareBeforeCreateMove(i_csgo_input* input) {
        if (!input || !CMenu::get()->IsMenuOpened())
            return;

        const uintptr_t inputAddr = reinterpret_cast<uintptr_t>(input);

        memory::Write<bool>(inputAddr + offsets::csgo_input::block_shot, true);
        memory::Write<int>(inputAddr + offsets::csgo_input::mouse_delta_x, 0);
        memory::Write<int>(inputAddr + offsets::csgo_input::mouse_delta_y, 0);
        memory::Write<float>(inputAddr + offsets::csgo_input::forward_move, 0.f);
        memory::Write<float>(inputAddr + offsets::csgo_input::left_move, 0.f);
        memory::Write<float>(inputAddr + offsets::csgo_input::up_move, 0.f);
    }

    void CleanupAfterCreateMove(i_csgo_input* input) {
        if (!input || !CMenu::get()->IsMenuOpened())
            return;

        const uintptr_t inputAddr = reinterpret_cast<uintptr_t>(input);
        StripInputButtons(inputAddr);

        memory::Write<int>(inputAddr + offsets::csgo_input::mouse_delta_x, 0);
        memory::Write<int>(inputAddr + offsets::csgo_input::mouse_delta_y, 0);

        void* controller = g_ctx ? g_ctx->m_local_controller : nullptr;
        if (!controller && g_interfaces && g_interfaces->m_entity_system)
            controller = g_interfaces->m_entity_system->get_local_controller();

        if (!controller)
            return;

        ClearCmdButtons(input->get_user_cmd(controller));
    }

    void RestoreWhenMenuClosed(i_csgo_input* input) {
        if (!input)
            return;

        memory::Write<bool>(reinterpret_cast<uintptr_t>(input) + offsets::csgo_input::block_shot, false);
    }
}
