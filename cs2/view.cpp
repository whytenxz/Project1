#include "view.h"
#include "offsets.h"
#include "../Backend/Config/Config.h"
#include "../nerv/main.hpp"
#include "../kiero/minhook/include/MinHook.h"

namespace view {
    namespace {
        using get_render_fov_fn = float(__fastcall*)(void*);
        get_render_fov_fn g_original_get_render_fov = nullptr;
        bool g_hooked = false;

        float __fastcall hk_get_render_fov(void* rcx) {
            CConfig* cfg = CConfig::get();
            if (cfg->b["misc_viewmodel_fov"])
                return cfg->f["misc_viewmodel_fov"];

            return g_original_get_render_fov ? g_original_get_render_fov(rcx) : 90.f;
        }
    }

    bool InitializeHook() {
        if (g_hooked)
            return true;

        if (!g_opcodes || !g_modules)
            return false;

        void* target = reinterpret_cast<void*>(
            g_opcodes->scan_absolute(
                g_modules->m_modules.client_dll.get_name(),
                offsets::signatures::get_render_fov,
                0x1));

        if (!target)
            return false;

        if (MH_CreateHook(target, &hk_get_render_fov, reinterpret_cast<void**>(&g_original_get_render_fov)) != MH_OK)
            return false;

        if (MH_EnableHook(target) != MH_OK)
            return false;

        g_hooked = true;
        return true;
    }
}
