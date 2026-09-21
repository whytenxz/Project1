#pragma once
#include "imgui/imgui.h"

namespace tracers {
    void Update(uintptr_t client);
    void Draw(ImDrawList* drawList, int screenWidth, int screenHeight);
}
