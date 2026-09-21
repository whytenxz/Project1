#include "streamer_mode.h"
#include "../Backend/Globalincludes.h"

#ifndef WDA_NONE
#define WDA_NONE 0x00000000
#endif
#ifndef WDA_EXCLUDEFROMCAPTURE
#define WDA_EXCLUDEFROMCAPTURE 0x00000011
#endif

namespace streamer_mode {
    namespace {
        bool g_enabled = false;
        bool g_affinityApplied = false;

        using set_window_display_affinity_fn = BOOL(WINAPI*)(HWND, DWORD);
        set_window_display_affinity_fn g_setAffinity = nullptr;

        set_window_display_affinity_fn GetSetWindowDisplayAffinity() {
            if (g_setAffinity)
                return g_setAffinity;

            HMODULE user32 = GetModuleHandleA("user32.dll");
            if (!user32)
                return nullptr;

            g_setAffinity = reinterpret_cast<set_window_display_affinity_fn>(
                GetProcAddress(user32, "SetWindowDisplayAffinity"));
            return g_setAffinity;
        }
    }

    void Update(bool enabled) {
        g_enabled = enabled;

        if (!g_GameWindow)
            return;

        auto setAffinity = GetSetWindowDisplayAffinity();
        if (!setAffinity)
            return;

        const DWORD affinity = enabled ? WDA_EXCLUDEFROMCAPTURE : WDA_NONE;
        if (g_affinityApplied != enabled) {
            setAffinity(g_GameWindow, affinity);
            g_affinityApplied = enabled;
        }
    }

    bool ShouldHideOverlays() {
        return g_enabled;
    }
}
