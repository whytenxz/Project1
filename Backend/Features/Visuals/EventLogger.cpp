#include "EventLogger.h"
#include "../../Config/Config.h"
#include "../../Utilities/Utilities.h"
#include "../../../Frontend/Renderer/Renderer.h"
#include "imgui/imgui.h"

#include <algorithm>

void Features::CEventLogger::AddLog(const char* str, ...)
{
    if (!str)
        return;

    char buffer[2048]{};
    va_list list;
    va_start(list, str);
    vsnprintf_s(buffer, sizeof(buffer), _TRUNCATE, str, list);
    va_end(list);

    EventLog_t entry{};
    entry.text = buffer;
    entry.alpha = 255;
    entry.lifetime = 4.f;
    logs.push_back(entry);

    while (logs.size() > 8)
        logs.erase(logs.begin());
}

void Features::CEventLogger::Draw()
{
    CConfig* cfg = CConfig::get();
    if (!cfg->b["misc_hit_logs"])
        return;

    const float dt = Misc::Utilities->GetDeltaTime();
    for (auto it = logs.begin(); it != logs.end();) {
        it->lifetime -= dt;
        if (it->lifetime <= 0.f)
            it = logs.erase(it);
        else
            ++it;
    }

    if (logs.empty())
        return;

    ImDrawList* drawList = ImGui::GetBackgroundDrawList();
    if (!drawList)
        return;

    ImFont* font = Render::Fonts::SmallFont ? Render::Fonts::SmallFont : ImGui::GetFont();
    const float fontSize = font->FontSize;
    const float screenH = ImGui::GetIO().DisplaySize.y;

    float y = screenH - 40.f;
    for (auto it = logs.rbegin(); it != logs.rend(); ++it) {
        const float fade = std::clamp(it->lifetime, 0.f, 1.f);
        const int alpha = static_cast<int>(220.f * fade);
        const ImU32 textColor = IM_COL32(255, 255, 255, alpha);
        const ImU32 accentColor = IM_COL32(163, 212, 31, alpha);

        const ImVec2 textSize = font->CalcTextSizeA(fontSize, FLT_MAX, 0.f, it->text.c_str());
        const float padX = 6.f;
        const float padY = 2.f;
        const float x = 12.f;

        drawList->AddRectFilled(
            ImVec2(x, y - padY),
            ImVec2(x + textSize.x + padX * 2.f + 3.f, y + textSize.y + padY),
            IM_COL32(12, 12, 12, static_cast<int>(180.f * fade)));
        drawList->AddRectFilled(
            ImVec2(x, y - padY),
            ImVec2(x + 2.f, y + textSize.y + padY),
            accentColor);
        drawList->AddText(font, fontSize, ImVec2(x + padX + 1.f, y + 1.f), IM_COL32(0, 0, 0, alpha / 2), it->text.c_str());
        drawList->AddText(font, fontSize, ImVec2(x + padX, y), textColor, it->text.c_str());

        y -= textSize.y + 6.f;
    }
}
