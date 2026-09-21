#pragma once
#include "imgui/imgui.h"

namespace esp {
    void Run(ImDrawList* drawList, int screenWidth, int screenHeight);
    void DrawKeybindList(int screenWidth, int screenHeight);
}
