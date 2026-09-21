#include "antiflash.h"
#include "offsets.h"
#include "../Backend/Config/Config.h"
#include "../nerv/main.hpp"
#include "../kiero/minhook/include/MinHook.h"

namespace antiflash {
    namespace {
        using render_flashbang_overlay_fn = void(__fastcall*)(void*, void*, void*, void*, void*);
        render_flashbang_overlay_fn g_original = nullptr;
        bool g_hooked = false;

        void __fastcall hk_render_flashbang_overlay(void* a1, void* a2, void* a3, void* a4, void* a5) {
            if (CConfig::get()->b["misc_antiflash"])
                return;

            if (g_original)
                g_original(a1, a2, a3, a4, a5);
        }
    }

    bool InitializeHook() {
        if (g_hooked)
            return true;

        if (!g_opcodes || !g_modules)
            return false;

        void* target = reinterpret_cast<void*>(
            g_opcodes->scan(
                g_modules->m_modules.client_dll.get_name(),
                offsets::signatures::render_flashbang_overlay));

        if (!target)
            return false;

        if (MH_CreateHook(target, &hk_render_flashbang_overlay, reinterpret_cast<void**>(&g_original)) != MH_OK)
            return false;

        if (MH_EnableHook(target) != MH_OK)
            return false;

        g_hooked = true;
        return true;
    }
}
