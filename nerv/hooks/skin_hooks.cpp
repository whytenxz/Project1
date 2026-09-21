#include "skin_hooks.hpp"
#include "../main.hpp"
#include "../features/shared/item_schema.hpp"
#include "../features/skin_changer/skin_changer.hpp"
#include "../features/glove_changer/glove_changer.hpp"
#include "../../cs2/agent_changer.h"
#include "../../cs2/bhop.h"
#include "../../cs2/model_changer.h"
#include "../../cs2/view.h"
#include "../../cs2/antiflash.h"

class c_hook {
    void* m_function = nullptr;
    void* m_detour = nullptr;
    void* m_trampoline = nullptr;
public:
    bool hook(void* target, void* dtr) {
        m_function = target;
        m_detour = dtr;

        if (MH_CreateHook(m_function, m_detour, &m_trampoline) != MH_OK)
            return false;
        return MH_EnableHook(m_function) == MH_OK;
    }

    template<typename tOriginal>
    tOriginal get_original() {
        return reinterpret_cast<tOriginal>(m_trampoline);
    }
};

namespace hooks {
    namespace frame_stage_notify {
        inline c_hook m_frame_stage_notify;

        void hk_frame_stage_notify(void* source_to_client, int stage) {
            auto original = m_frame_stage_notify.get_original<void(__fastcall*)(void*, int)>();

            if (g_interfaces && g_interfaces->m_entity_system) {
                g_ctx->m_local_pawn = g_interfaces->m_entity_system->get_local_pawn();
                g_ctx->m_local_controller = g_interfaces->m_entity_system->get_local_controller();
            }

            model_changer::OnFrameStage(stage);
            agent_changer::OnFrameStage(stage);

            if (stage == 7) {
                g_skin_changer->run(stage);
                g_glove_changer->run(stage);
            }

            original(source_to_client, stage);
        }
    }

    namespace level_init {
        inline c_hook m_level_init;

        __int64 hk_level_init(void* rcx, void* rdx) {
            auto original = m_level_init.get_original<__int64(__fastcall*)(void*, void*)>();

            if (g_cfg->knife_changer.m_enabled || g_cfg->skin_changer.m_enabled)
                g_skin_changer->should_update = true;
            if (g_cfg->glove_changer.m_enabled)
                g_glove_changer->should_update = true;

            model_changer::OnLevelInit();
            agent_changer::OnLevelInit();

            return original(rcx, rdx);
        }
    }
}

bool c_skin_hooks::initialize() {
    const char* client_dll = g_modules->m_modules.client_dll.get_name();

    const auto frame_stage = g_opcodes->scan(client_dll, "48 89 5C 24 ? 48 89 6C 24 ? 57 48 83 EC ? 48 8B F9 33 ED");
    if (!frame_stage)
        return false;

    if (!hooks::frame_stage_notify::m_frame_stage_notify.hook(frame_stage, hooks::frame_stage_notify::hk_frame_stage_notify))
        return false;

    const auto level_init = g_opcodes->scan(client_dll, "40 55 56 41 56 48 8D 6C 24 ? 48 81 EC ? ? ? ? 48 8B 0D");
    if (level_init)
        hooks::level_init::m_level_init.hook(level_init, hooks::level_init::hk_level_init);

    bhop::InitializeHook();
    view::InitializeHook();
    antiflash::InitializeHook();

    return true;
}
