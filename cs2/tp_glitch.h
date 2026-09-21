#pragma once

#include <cstdint>

struct ImDrawList;

namespace tp_glitch {
    void Tick(uintptr_t client);
    void ApplyView(uintptr_t client);
    void Draw(ImDrawList* drawList, int screenWidth, int screenHeight);
    bool IsMarkerVisible();
    bool IsViewGlitchActive();
}
