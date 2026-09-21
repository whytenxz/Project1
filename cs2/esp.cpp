#include "esp.h"
#include "tp_glitch.h"
#include "game_state.h"
#include "memory.h"
#include "math.h"
#include "offsets.h"
#include "players.h"
#include "weapons.h"
#include "../Backend/Config/Config.h"
#include "../Backend/Features/Visuals/EventLogger.h"
#include "../Frontend/Menu/Menu.h"
#include "../Frontend/Renderer/Renderer.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <ctime>
#include <string>
#include <unordered_map>
#include <vector>

namespace esp {
    namespace {
        constexpr ImU32 kWatermarkGreen = IM_COL32(163, 212, 31, 255);
        constexpr ImU32 kWatermarkWhite = IM_COL32(255, 255, 255, 255);
        constexpr ImU32 kWatermarkBg = IM_COL32(12, 12, 12, 200);
        constexpr ImU32 kWatermarkBorder = IM_COL32(0, 0, 0, 255);
        constexpr ImU32 kWatermarkTopLine = IM_COL32(45, 45, 45, 255);

        struct PlayerState {
            int health = 0;
            bool valid = false;
        };

        std::array<PlayerState, 65> g_playerStates{};
        int g_lastLocalShots = 0;
        std::chrono::steady_clock::time_point g_lastShotTime{};

        float GetCurTime(uintptr_t client) {
            const uintptr_t globalVars = memory::Read<uintptr_t>(client + offsets::client_dll::dwGlobalVars);
            if (!globalVars)
                return 0.f;
            return memory::Read<float>(globalVars + offsets::global_vars::curtime);
        }

        bool IsBombPlanted(uintptr_t client) {
            const uintptr_t gameRulesProxy = memory::Read<uintptr_t>(client + offsets::client_dll::dwGameRules);
            if (!gameRulesProxy)
                return false;

            const uintptr_t gameRules = memory::Read<uintptr_t>(gameRulesProxy + schema::C_CSGameRulesProxy::m_pGameRules);
            if (!gameRules)
                return false;

            return memory::Read<bool>(gameRules + schema::C_CSGameRules::m_bBombPlanted);
        }

        bool IsValidPlantedC4(uintptr_t plantedC4) {
            if (!plantedC4)
                return false;
            if (memory::Read<bool>(plantedC4 + schema::C_PlantedC4::m_bBombDefused))
                return false;
            if (memory::Read<bool>(plantedC4 + schema::C_PlantedC4::m_bHasExploded))
                return false;
            return true;
        }

        uintptr_t FindPlantedC4(uintptr_t client) {
            const uintptr_t plantedC4Ptr = memory::Read<uintptr_t>(client + offsets::client_dll::dwPlantedC4);
            if (!plantedC4Ptr)
                return 0;

            const uintptr_t plantedC4 = memory::Read<uintptr_t>(plantedC4Ptr);
            if (!IsValidPlantedC4(plantedC4))
                return 0;

            if (!memory::Read<bool>(plantedC4 + schema::C_PlantedC4::m_bBombTicking))
                return 0;

            return plantedC4;
        }

        ImU32 ConfigColor(const int col[4], int alphaOverride = -1) {
            return IM_COL32(
                col[0],
                col[1],
                col[2],
                alphaOverride >= 0 ? alphaOverride : col[3]);
        }

        void DrawShadowText(ImDrawList* drawList, ImFont* font, float size, ImVec2 pos, ImU32 color, const char* text) {
            drawList->AddText(font, size, ImVec2(pos.x + 1.f, pos.y + 1.f), IM_COL32(0, 0, 0, 180), text);
            drawList->AddText(font, size, pos, color, text);
        }

        void DrawOutlinedText(ImDrawList* drawList, ImVec2 pos, ImU32 color, const char* text) {
            drawList->AddText(ImVec2(pos.x + 1.f, pos.y + 1.f), IM_COL32(0, 0, 0, 255), text);
            drawList->AddText(ImVec2(pos.x - 1.f, pos.y - 1.f), IM_COL32(0, 0, 0, 255), text);
            drawList->AddText(ImVec2(pos.x + 1.f, pos.y - 1.f), IM_COL32(0, 0, 0, 255), text);
            drawList->AddText(ImVec2(pos.x - 1.f, pos.y + 1.f), IM_COL32(0, 0, 0, 255), text);
            drawList->AddText(pos, color, text);
        }

        void DrawBox(ImDrawList* drawList, float x, float y, float w, float h, ImU32 color) {
            drawList->AddRect(ImVec2(x - 1, y - 1), ImVec2(x + w + 1, y + h + 1), IM_COL32(0, 0, 0, 180));
            drawList->AddRect(ImVec2(x, y), ImVec2(x + w, y + h), color);
            drawList->AddRect(ImVec2(x + 1, y + 1), ImVec2(x + w - 1, y + h - 1), IM_COL32(0, 0, 0, 180));
        }

        void DrawCornerBox(ImDrawList* drawList, float x, float y, float w, float h, ImU32 color) {
            const float lineW = w * 0.25f;
            const float lineH = h * 0.25f;

            auto corner = [&](float cx, float cy, float dx, float dy) {
                drawList->AddLine(ImVec2(cx, cy), ImVec2(cx + dx, cy), color);
                drawList->AddLine(ImVec2(cx, cy), ImVec2(cx, cy + dy), color);
            };

            corner(x, y, lineW, lineH);
            corner(x + w, y, -lineW, lineH);
            corner(x, y + h, lineW, -lineH);
            corner(x + w, y + h, -lineW, -lineH);
        }

        void DrawHealthBar(ImDrawList* drawList, float x, float y, float h, int health) {
            health = std::clamp(health, 0, 100);
            const float ratio = health / 100.0f;
            const float offset = h - (h * ratio);

            const int r = static_cast<int>(244 - (116 * ratio));
            const int g = static_cast<int>(100 + (144 * ratio));
            const ImU32 color = IM_COL32(r, g, 66, 220);

            drawList->AddRectFilled(ImVec2(x - 5, y), ImVec2(x - 1, y + h), IM_COL32(0, 0, 0, 130));
            drawList->AddRectFilled(ImVec2(x - 4, y + 1 + offset), ImVec2(x - 2, y + h - 1), color);
        }

        void DrawArmorBar(ImDrawList* drawList, float x, float y, float w, float h, int armor) {
            armor = std::clamp(armor, 0, 100);
            const float ratio = armor / 100.0f;
            const ImU32 color = IM_COL32(90, 140, 220, 220);

            const float barY = y + h + 3.f;
            drawList->AddRectFilled(ImVec2(x, barY), ImVec2(x + w, barY + 3.f), IM_COL32(0, 0, 0, 130));
            drawList->AddRectFilled(ImVec2(x + 1, barY + 1), ImVec2(x + 1 + (w - 2.f) * ratio, barY + 2.f), color);
        }

        bool IsBoxOnScreen(float x, float y, float w, float h, const Vector2& head, int screenWidth, int screenHeight) {
            if (head.x < 0.f || head.x > static_cast<float>(screenWidth))
                return false;
            if (head.y < 0.f || head.y > static_cast<float>(screenHeight))
                return false;

            const float visibleW = (std::min)(x + w, static_cast<float>(screenWidth)) - (std::max)(x, 0.f);
            const float visibleH = (std::min)(y + h, static_cast<float>(screenHeight)) - (std::max)(y, 0.f);
            return visibleW > 2.f && visibleH > 2.f;
        }

        bool GetPlayerBounds(uintptr_t pawn, const float* viewMatrix, int width, int height, float& outX, float& outY, float& outW, float& outH, Vector2* outHead = nullptr) {
            const Vector3 origin = memory::Read<Vector3>(pawn + schema::C_BaseEntity::m_vOldOrigin);
            const Vector3 head = origin + Vector3{ 0.f, 0.f, 72.f };
            const Vector3 feet = origin + Vector3{ 0.f, 0.f, 0.f };

            Vector2 screenHead{}, screenFeet{};
            if (!::WorldToScreen(head, screenHead, viewMatrix, width, height))
                return false;
            if (!::WorldToScreen(feet, screenFeet, viewMatrix, width, height))
                return false;

            const float boxH = screenFeet.y - screenHead.y;
            if (boxH < 2.f)
                return false;

            const float boxW = boxH * 0.5f;
            outX = screenHead.x - boxW * 0.5f;
            outY = screenHead.y;
            outW = boxW;
            outH = boxH;

            if (outHead)
                *outHead = screenHead;

            return true;
        }

        bool GetBonePosition(uintptr_t pawn, int boneIndex, Vector3& out) {
            const uintptr_t sceneNode = memory::Read<uintptr_t>(pawn + schema::C_BaseEntity::m_pGameSceneNode);
            if (!sceneNode)
                return false;

            const uintptr_t modelState = sceneNode + schema::CSkeletonInstance::m_modelState;
            const uintptr_t boneArray = memory::Read<uintptr_t>(modelState + schema::CModelState::m_boneArray);
            if (!boneArray)
                return false;

            out = memory::Read<Vector3>(boneArray + static_cast<uintptr_t>(boneIndex) * 32);
            return std::isfinite(out.x) && std::isfinite(out.y) && std::isfinite(out.z);
        }

        Vector2 CatmullRom(const Vector2& p0, const Vector2& p1, const Vector2& p2, const Vector2& p3, float t) {
            const float t2 = t * t;
            const float t3 = t2 * t;
            return {
                0.5f * ((2.0f * p1.x) + (-p0.x + p2.x) * t + (2.0f * p0.x - 5.0f * p1.x + 4.0f * p2.x - p3.x) * t2 + (-p0.x + 3.0f * p1.x - 3.0f * p2.x + p3.x) * t3),
                0.5f * ((2.0f * p1.y) + (-p0.y + p2.y) * t + (2.0f * p0.y - 5.0f * p1.y + 4.0f * p2.y - p3.y) * t2 + (-p0.y + 3.0f * p1.y - 3.0f * p2.y + p3.y) * t3)
            };
        }

        void DrawSkeleton(ImDrawList* drawList, uintptr_t pawn, const float* viewMatrix, int screenWidth, int screenHeight, ImU32 color) {
            static const std::array<std::array<int, 4>, 5> kBoneChains = { {
                { 6, 5, 4, 0 },    // head -> pelvis
                { 4, 8, 9, 11 },   // left arm
                { 4, 13, 14, 16 }, // right arm
                { 0, 22, 23, 24 }, // left leg
                { 0, 25, 26, 27 }  // right leg
            } };

            constexpr float kThickness = 1.5f;
            constexpr float kStep = 0.08f;

            for (const auto& chain : kBoneChains) {
                std::vector<Vector2> points;
                points.reserve(chain.size());

                for (int boneIndex : chain) {
                    Vector3 bonePos{};
                    if (!GetBonePosition(pawn, boneIndex, bonePos))
                        continue;

                    Vector2 screen{};
                    if (!::WorldToScreen(bonePos, screen, viewMatrix, screenWidth, screenHeight))
                        continue;

                    points.push_back(screen);
                }

                if (points.size() < 2)
                    continue;

                points.insert(points.begin(), points.front());
                points.push_back(points.back());

                Vector2 last{};
                bool first = true;

                for (size_t i = 0; i + 3 < points.size(); ++i) {
                    const Vector2& p0 = points[i];
                    const Vector2& p1 = points[i + 1];
                    const Vector2& p2 = points[i + 2];
                    const Vector2& p3 = points[i + 3];

                    for (float t = 0.f; t <= 1.f; t += kStep) {
                        const Vector2 pt = CatmullRom(p0, p1, p2, p3, t);

                        if (first) {
                            last = pt;
                            first = false;
                            continue;
                        }

                        drawList->AddLine(
                            ImVec2(last.x, last.y),
                            ImVec2(pt.x, pt.y),
                            color,
                            kThickness);

                        last = pt;
                    }
                }
            }
        }

        void DrawOofArrow(ImDrawList* drawList, const Vector2& targetScreen, int screenWidth, int screenHeight, ImU32 color, int radius, int size) {
            const ImVec2 center(screenWidth * 0.5f, screenHeight * 0.5f);
            ImVec2 dir(targetScreen.x - center.x, targetScreen.y - center.y);

            const float len = std::sqrt(dir.x * dir.x + dir.y * dir.y);
            if (len < 1.f)
                return;

            dir.x /= len;
            dir.y /= len;

            const ImVec2 tip(center.x + dir.x * static_cast<float>(radius), center.y + dir.y * static_cast<float>(radius));
            const ImVec2 perp(-dir.y, dir.x);
            const float half = static_cast<float>(size) * 0.5f;

            const ImVec2 p1(tip.x - dir.x * static_cast<float>(size), tip.y - dir.y * static_cast<float>(size));
            const ImVec2 p2(tip.x + perp.x * half, tip.y + perp.y * half);
            const ImVec2 p3(tip.x - perp.x * half, tip.y - perp.y * half);

            drawList->AddTriangleFilled(p1, p2, p3, color);
            drawList->AddTriangle(p1, p2, p3, IM_COL32(0, 0, 0, 200), 1.f);
        }

        void DrawWatermark(ImDrawList* drawList, int screenWidth, uintptr_t client, uintptr_t localController) {
            CConfig* cfg = CConfig::get();
            if (!cfg->b["misc_watermark"])
                return;

            ImFont* font = Render::Fonts::WatermarkFont ? Render::Fonts::WatermarkFont : ImGui::GetFont();
            const float fontSize = font->FontSize;

            struct Segment {
                std::string text;
                ImU32 color;
            };

            std::vector<Segment> segments;
            segments.push_back({ "gamesense", kWatermarkWhite });

            if (cfg->b["misc_watermark_username"] && localController) {
                std::string username = memory::ReadString(localController + schema::CBasePlayerController::m_iszPlayerName);
                if (!username.empty())
                    segments.push_back({ username, kWatermarkWhite });
            }

            if (cfg->b["misc_watermark_fps"]) {
                const int fps = static_cast<int>(ImGui::GetIO().Framerate + 0.5f);
                segments.push_back({ std::to_string(fps), kWatermarkGreen });
                segments.push_back({ "FPS", kWatermarkWhite });
            }

            if (cfg->b["misc_watermark_time"]) {
                const std::time_t now = std::time(nullptr);
                std::tm localTime{};
                localtime_s(&localTime, &now);
                char timeBuf[16]{};
                std::strftime(timeBuf, sizeof(timeBuf), "%H:%M:%S", &localTime);
                segments.push_back({ timeBuf, kWatermarkWhite });
            }

            if (cfg->b["misc_watermark_ping"]) {
                segments.push_back({ std::to_string(cfg->i["misc_watermark_ping_value"]) + "ms", kWatermarkWhite });
            }

            constexpr float padX = 10.f;
            constexpr float padY = 5.f;
            constexpr float gap = 18.f;

            float totalWidth = padX * 2.f;
            for (size_t i = 0; i < segments.size(); ++i) {
                const ImVec2 size = font->CalcTextSizeA(fontSize, FLT_MAX, 0.f, segments[i].text.c_str());
                totalWidth += size.x;
                if (i + 1 < segments.size())
                    totalWidth += gap;
            }

            const float barHeight = fontSize + padY * 2.f;
            const float x = static_cast<float>(screenWidth) - totalWidth;
            const float y = 0.f;

            drawList->AddRectFilled(ImVec2(x, y), ImVec2(x + totalWidth, y + barHeight), kWatermarkBg);
            drawList->AddLine(ImVec2(x, y), ImVec2(x + totalWidth, y), kWatermarkTopLine);
            drawList->AddRect(ImVec2(x, y), ImVec2(x + totalWidth, y + barHeight), kWatermarkBorder);

            float cursorX = x + padX;
            const float textY = y + padY;
            for (const auto& segment : segments) {
                drawList->AddText(font, fontSize, ImVec2(cursorX, textY), segment.color, segment.text.c_str());
                cursorX += font->CalcTextSizeA(fontSize, FLT_MAX, 0.f, segment.text.c_str()).x + gap;
            }
        }

        void DrawBombTimer(ImDrawList* drawList, int screenWidth, int screenHeight, uintptr_t client, uintptr_t localPawn, const float* viewMatrix) {
            CConfig* cfg = CConfig::get();
            if (!cfg->b["visuals_bomb_timer"] || !localPawn || !viewMatrix)
                return;

            const uintptr_t plantedC4Ptr = memory::Read<uintptr_t>(client + offsets::client_dll::dwPlantedC4);
            if (!plantedC4Ptr)
                return;

            const uintptr_t plantedC4 = memory::Read<uintptr_t>(plantedC4Ptr);
            if (!IsValidPlantedC4(plantedC4))
                return;

            if (!memory::Read<bool>(plantedC4 + schema::C_PlantedC4::m_bBombTicking))
                return;

            const uintptr_t bombNode = memory::Read<uintptr_t>(plantedC4 + schema::C_BaseEntity::m_pGameSceneNode);
            if (!bombNode)
                return;

            const Vector3 bombOrigin = memory::Read<Vector3>(bombNode + schema::CGameSceneNode::m_vecAbsOrigin);
            if (bombOrigin.x == 0.f && bombOrigin.y == 0.f && bombOrigin.z == 0.f)
                return;

            Vector2 bombScreen{};
            if (!::WorldToScreen(bombOrigin, bombScreen, viewMatrix, screenWidth, screenHeight))
                return;

            const float currentTime = memory::Read<float>(localPawn + schema::C_BaseEntity::m_flSimulationTime);
            const float blowTime = memory::Read<float>(plantedC4 + schema::C_PlantedC4::m_flC4Blow);
            const float remaining = blowTime - currentTime;
            if (remaining <= 0.f)
                return;

            char bombText[64]{};
            snprintf(bombText, sizeof(bombText), "C4: %.1f", remaining);

            const ImVec2 textSize = ImGui::CalcTextSize(bombText);
            const ImVec2 textPos(bombScreen.x - textSize.x * 0.5f, bombScreen.y);
            const ImU32 color = remaining < 10.f ? IM_COL32(255, 0, 0, 255) : IM_COL32(255, 255, 0, 255);

            drawList->AddText(ImVec2(textPos.x + 1.f, textPos.y + 1.f), IM_COL32(0, 0, 0, 255), bombText);
            drawList->AddText(textPos, color, bombText);
        }

        bool IsSpectatorListEnabled(CConfig* cfg) {
            return cfg->b["visuals_spectator_list"] || cfg->b["misc_spectator_list"];
        }

        void DrawSpectatorList(ImDrawList* drawList, int /*screenWidth*/, int /*screenHeight*/) {
            CConfig* cfg = CConfig::get();
            if (!IsSpectatorListEnabled(cfg))
                return;

            std::vector<std::string> spectators;
            players::GatherSpectators(spectators);
            if (spectators.empty())
                return;

            float specY = 20.f;
            for (const auto& spectator : spectators) {
                char specText[64]{};
                snprintf(specText, sizeof(specText), "[Spectator] %s", spectator.c_str());
                DrawOutlinedText(drawList, ImVec2(10.f, specY), IM_COL32(255, 0, 0, 255), specText);
                specY += 15.f;
            }
        }

        void ApplyGlow(uintptr_t entityList, uintptr_t localPawn, int localTeam, CConfig* cfg) {
            if (!cfg->b["visuals_player_esp_glow"])
                return;

            const int col[4] = {
                cfg->c["visuals_player_esp_glow_color"][0],
                cfg->c["visuals_player_esp_glow_color"][1],
                cfg->c["visuals_player_esp_glow_color"][2],
                cfg->c["visuals_player_esp_glow_color"][3]
            };
            const uint32_t colorInt =
                static_cast<uint32_t>(col[0])
                | (static_cast<uint32_t>(col[1]) << 8)
                | (static_cast<uint32_t>(col[2]) << 16)
                | (static_cast<uint32_t>(col[3]) << 24);

            for (int i = 1; i <= 64; ++i) {
                const uintptr_t controller = memory::GetEntityByIndex(entityList, i);
                if (!controller)
                    continue;

                if (!memory::Read<bool>(controller + schema::CCSPlayerController::m_bPawnIsAlive))
                    continue;

                const uint32_t pawnHandle = memory::Read<uint32_t>(controller + schema::CCSPlayerController::m_hPlayerPawn);
                const uintptr_t pawn = memory::ResolveHandle(entityList, pawnHandle);
                if (!pawn || pawn == localPawn)
                    continue;

                if (memory::Read<int>(pawn + schema::C_BaseEntity::m_iHealth) <= 0)
                    continue;

                const int team = memory::Read<uint8_t>(pawn + schema::C_BaseEntity::m_iTeamNum);
                if (team == localTeam)
                    continue;

                const uintptr_t glow = pawn + schema::C_BaseModelEntity::m_Glow;
                memory::Write<bool>(glow + schema::CGlowProperty::m_bGlowing, true);
                memory::Write<int>(glow + schema::CGlowProperty::m_iGlowType, 3);
                memory::Write<uint32_t>(glow + schema::CGlowProperty::m_glowColorOverride, colorInt);
            }
        }

        void TrackHits(int playerIndex, int health, int team, int localTeam, const std::string& name, bool localRecentlyShot) {
            CConfig* cfg = CConfig::get();
            if (!cfg->b["misc_hit_logs"] || team == localTeam)
                return;

            auto& state = g_playerStates[playerIndex];
            if (state.valid && health < state.health && localRecentlyShot) {
                const int damage = state.health - health;
                if (health <= 0)
                    Features::EventLogger->AddLog("Killed %s for %d damage", name.c_str(), damage);
                else
                    Features::EventLogger->AddLog("Hit %s for %d damage (%d health remaining)", name.c_str(), damage, health);
            }

            state.health = health;
            state.valid = true;
        }
    }

    void Run(ImDrawList* drawList, int screenWidth, int screenHeight) {
        if (!drawList)
            return;

        const uintptr_t client = memory::GetModuleBase("client.dll");
        const uintptr_t localController = client ? memory::Read<uintptr_t>(client + offsets::client_dll::dwLocalPlayerController) : 0;

        DrawWatermark(drawList, screenWidth, client, localController);

        CConfig* cfg = CConfig::get();

        float viewMatrix[16]{};
        if (client) {
            for (int i = 0; i < 16; ++i)
                viewMatrix[i] = memory::Read<float>(client + offsets::client_dll::dwViewMatrix + i * sizeof(float));
        }

        const bool inMatch = game_state::IsInMatch();

        if (client && inMatch)
            tp_glitch::Tick(client);

        if (client && inMatch) {
            uintptr_t entityList = 0;
            uintptr_t localControllerIgnored = 0;
            uintptr_t matchLocalPawn = 0;
            if (game_state::GetClientPointers(client, entityList, localControllerIgnored, matchLocalPawn)) {
                DrawBombTimer(drawList, screenWidth, screenHeight, client, matchLocalPawn, viewMatrix);
                DrawSpectatorList(drawList, screenWidth, screenHeight);

                const int localTeam = memory::Read<uint8_t>(matchLocalPawn + schema::C_BaseEntity::m_iTeamNum);
                ApplyGlow(entityList, matchLocalPawn, localTeam, cfg);
            }
        }

        if (!cfg->IsBindActive("visuals_player_esp_activation_type_key"))
            return;

        if (!client || !inMatch)
            return;

        uintptr_t entityList = 0;
        uintptr_t localControllerIgnored = 0;
        uintptr_t matchLocalPawn = 0;
        if (!game_state::GetClientPointers(client, entityList, localControllerIgnored, matchLocalPawn))
            return;

        const uintptr_t localPawn = matchLocalPawn;

        const int localTeam = memory::Read<uint8_t>(localPawn + schema::C_BaseEntity::m_iTeamNum);
        const Vector3 localOrigin = memory::Read<Vector3>(localPawn + schema::C_BaseEntity::m_vOldOrigin);
        const int maxDistance = (std::max)(cfg->i["visuals_player_esp_max_distance"], 100);

        const int localShots = memory::Read<int>(localPawn + schema::C_CSPlayerPawn::m_iShotsFired);
        if (localShots > g_lastLocalShots)
            g_lastShotTime = std::chrono::steady_clock::now();
        g_lastLocalShots = localShots;

        const auto now = std::chrono::steady_clock::now();
        const bool localRecentlyShot = std::chrono::duration_cast<std::chrono::milliseconds>(now - g_lastShotTime).count() < 400;

        const ImU32 boxColor = ConfigColor(cfg->c["visuals_player_esp_bounding_box_color"]);
        const ImU32 nameColor = ConfigColor(cfg->c["visuals_player_esp_name_color"]);
        const ImU32 snapColor = ConfigColor(cfg->c["visuals_player_esp_snaplines_color"]);
        const ImU32 headColor = ConfigColor(cfg->c["visuals_player_esp_head_dot_color"]);
        const ImU32 fillColor = ConfigColor(cfg->c["visuals_player_esp_filled_box_color"], 40);
        const ImU32 weaponColor = ConfigColor(cfg->c["visuals_player_esp_weapon_color"]);
        const ImU32 oofColor = ConfigColor(cfg->c["visuals_oof_arrows_color"]);
        const ImU32 skeletonColor = ConfigColor(cfg->c["visuals_player_esp_skeleton_color"]);

        for (int i = 1; i <= 64; ++i) {
            const uintptr_t controller = memory::GetEntityByIndex(entityList, i);
            if (!controller) {
                g_playerStates[i].valid = false;
                continue;
            }

            if (!memory::Read<bool>(controller + schema::CCSPlayerController::m_bPawnIsAlive)) {
                g_playerStates[i].valid = false;
                continue;
            }

            const uint32_t pawnHandle = memory::Read<uint32_t>(controller + schema::CCSPlayerController::m_hPlayerPawn);
            const uintptr_t pawn = memory::ResolveHandle(entityList, pawnHandle);
            if (!pawn || pawn == localPawn) {
                g_playerStates[i].valid = false;
                continue;
            }

            if (memory::Read<uint8_t>(pawn + schema::C_BaseEntity::m_lifeState) != 0) {
                g_playerStates[i].valid = false;
                continue;
            }

            const int health = memory::Read<int>(pawn + schema::C_BaseEntity::m_iHealth);
            if (health <= 0) {
                g_playerStates[i].valid = false;
                continue;
            }

            const int team = memory::Read<uint8_t>(pawn + schema::C_BaseEntity::m_iTeamNum);
            if (team == localTeam && !cfg->b["visuals_player_esp_teammates"])
                continue;

            std::string name = memory::ReadString(controller + schema::CBasePlayerController::m_iszPlayerName);
            if (name.empty())
                name = "player";

            TrackHits(i, health, team, localTeam, name, localRecentlyShot);

            const Vector3 origin = memory::Read<Vector3>(pawn + schema::C_BaseEntity::m_vOldOrigin);
            const float dx = origin.x - localOrigin.x;
            const float dy = origin.y - localOrigin.y;
            const float dz = origin.z - localOrigin.z;
            const int distance = static_cast<int>(std::sqrt(dx * dx + dy * dy + dz * dz) * 0.0254f);
            if (distance > maxDistance)
                continue;

            float x, y, w, h;
            Vector2 screenHead{};
            if (!GetPlayerBounds(pawn, viewMatrix, screenWidth, screenHeight, x, y, w, h, &screenHead))
                continue;

            const bool onScreen = IsBoxOnScreen(x, y, w, h, screenHead, screenWidth, screenHeight);

            if (!onScreen) {
                if (cfg->b["visuals_oof_arrows"]) {
                    Vector2 targetScreen{};
                    const Vector3 targetPoint = origin + Vector3{ 0.f, 0.f, 36.f };
                    if (::WorldToScreen(targetPoint, targetScreen, viewMatrix, screenWidth, screenHeight)) {
                        DrawOofArrow(
                            drawList,
                            targetScreen,
                            screenWidth,
                            screenHeight,
                            oofColor,
                            cfg->i["visuals_oof_arrows_radius"],
                            cfg->i["visuals_oof_arrows_size"]);
                    }
                }
                continue;
            }

            if (cfg->b["visuals_player_esp_filled_box"])
                drawList->AddRectFilled(ImVec2(x, y), ImVec2(x + w, y + h), fillColor);

            if (cfg->b["visuals_player_esp_bounding_box"]) {
                if (cfg->i["visuals_player_esp_box_style"] == 1)
                    DrawCornerBox(drawList, x, y, w, h, boxColor);
                else
                    DrawBox(drawList, x, y, w, h, boxColor);
            }

            if (cfg->b["visuals_player_esp_health_bar"])
                DrawHealthBar(drawList, x, y, h, health);

            if (cfg->b["visuals_player_esp_armor_bar"]) {
                const int armor = memory::Read<int>(pawn + schema::C_CSPlayerPawn::m_ArmorValue);
                if (armor > 0)
                    DrawArmorBar(drawList, x, y, w, h, armor);
            }

            if (cfg->b["visuals_player_esp_health_text"]) {
                const std::string hp = std::to_string(health);
                const ImVec2 textSize = ImGui::CalcTextSize(hp.c_str());
                const float textX = x - 8.f - textSize.x;
                const float textY = y + (h - textSize.y) * 0.5f;
                drawList->AddText(ImVec2(textX + 1, textY + 1), IM_COL32(0, 0, 0, 200), hp.c_str());
                drawList->AddText(ImVec2(textX, textY), IM_COL32(255, 255, 255, 220), hp.c_str());
            }

            if (cfg->b["visuals_player_esp_name"]) {
                const ImVec2 textSize = ImGui::CalcTextSize(name.c_str());
                const float textX = x + (w - textSize.x) * 0.5f;
                const float textY = y - textSize.y - 2.f;

                drawList->AddText(ImVec2(textX + 1, textY + 1), IM_COL32(0, 0, 0, 200), name.c_str());
                drawList->AddText(ImVec2(textX, textY), nameColor, name.c_str());
            }

            if (cfg->b["visuals_player_esp_weapon"]) {
                const uint16_t defIndex = weapons::GetActiveWeaponDefIndex(entityList, pawn);
                const char* weaponName = weapons::GetName(defIndex);
                if (weaponName) {
                    const ImVec2 textSize = ImGui::CalcTextSize(weaponName);
                    float textY = y + h + 4.f;
                    if (cfg->b["visuals_player_esp_distance"])
                        textY += ImGui::CalcTextSize("[ 999m ]").y + 2.f;

                    const float textX = x + (w - textSize.x) * 0.5f;
                    DrawOutlinedText(drawList, ImVec2(textX, textY), weaponColor, weaponName);
                }
            }

            if (cfg->b["visuals_player_esp_distance"]) {
                char distBuf[32]{};
                snprintf(distBuf, sizeof(distBuf), "[ %dm ]", distance);
                const ImVec2 textSize = ImGui::CalcTextSize(distBuf);
                const float textX = x + (w - textSize.x) * 0.5f;
                const float textY = y + h + 4.f;
                DrawOutlinedText(drawList, ImVec2(textX, textY), IM_COL32(255, 255, 255, 255), distBuf);
            }

            if (cfg->b["visuals_player_esp_snaplines"]) {
                drawList->AddLine(
                    ImVec2(screenWidth * 0.5f, static_cast<float>(screenHeight)),
                    ImVec2(screenHead.x, screenHead.y),
                    snapColor);
            }

            if (cfg->b["visuals_player_esp_head_dot"])
                drawList->AddCircleFilled(ImVec2(screenHead.x, screenHead.y), 3.f, headColor);

            if (cfg->b["visuals_player_esp_skeleton"])
                DrawSkeleton(drawList, pawn, viewMatrix, screenWidth, screenHeight, skeletonColor);

            if (cfg->b["visuals_player_esp_flags"]) {
                float flagY = y;
                const auto drawFlag = [&](const char* text) {
                    drawList->AddText(ImVec2(x + w + 4, flagY), IM_COL32(255, 255, 255, 220), text);
                    flagY += ImGui::CalcTextSize(text).y + 1.f;
                };

                if (memory::Read<bool>(pawn + schema::C_CSPlayerPawn::m_bIsScoped))
                    drawFlag("SCOPED");
                if (memory::Read<bool>(pawn + schema::C_CSPlayerPawn::m_bIsDefusing))
                    drawFlag("DEFUSE");
                if (health <= 30)
                    drawFlag("LOW");
            }
        }

        tp_glitch::Draw(drawList, screenWidth, screenHeight);
    }

    namespace {
        const char* GetKeyName(int vk) {
            static const char* keys[] = {
                "[-]", "[M1]", "[M2]", "[CN]", "[M3]", "[M4]", "[M5]", "[-]", "[BCK]", "[TAB]",
                "[-]", "[-]", "[CLR]", "[RET]", "[-]", "[-]", "[SHI]", "[CTR]", "[ALT]", "[PAU]",
                "[CAP]", "[KAN]", "[-]", "[JUN]", "[FIN]", "[KAN]", "[-]", "[ESC]", "[CON]", "[NCO]",
                "[ACC]", "[MAD]", "[SPA]", "[PGU]", "[PGD]", "[END]", "[HOM]", "[LEF]", "[UP]", "[RIG]",
                "[DOW]", "[SEL]", "[PRI]", "[EXE]", "[PRI]", "[INS]", "[DEL]", "[HEL]", "[0]", "[1]",
                "[2]", "[3]", "[4]", "[5]", "[6]", "[7]", "[8]", "[9]", "[0]", "[-]", "[-]", "[-]",
                "[-]", "[-]", "[-]", "[A]", "[B]", "[C]", "[D]", "[E]", "[F]", "[G]", "[H]", "[I]",
                "[J]", "[K]", "[L]", "[M]", "[N]", "[O]", "[P]", "[Q]", "[R]", "[S]", "[T]", "[U]",
                "[V]", "[W]", "[X]", "[Y]", "[Z]", "[WIN]", "[WIN]", "[5D]", "[-]", "[SLE]", "[KP0]",
                "[KP1]", "[KP2]", "[KP3]", "[KP4]", "[KP5]", "[KP6]", "[KP7]", "[KP8]", "[KP9]",
                "[KP*]", "[KP+]", "[SEP]", "[KP-]", "[KP.]", "[KP/]", "[F1]", "[F2]", "[F3]", "[F4]",
                "[F5]", "[F6]", "[F7]", "[F8]", "[F9]", "[F10]", "[F11]", "[F12]", "[F13]", "[F14]",
                "[F15]", "[F16]", "[F17]", "[F18]", "[F19]", "[F20]", "[F21]", "[F22]", "[F23]", "[F24]",
                "[-]", "[-]", "[-]", "[-]", "[-]", "[-]", "[-]", "[-]", "[NUM]", "[SCR]", "[EQU]",
                "[MAS]", "[TOY]", "[OYA]", "[OYA]", "[-]", "[-]", "[-]", "[-]", "[-]", "[-]", "[-]",
                "[-]", "[-]", "[SHI]", "[SHI]", "[CTR]", "[CTR]", "[ALT]", "[ALT]"
            };

            if (vk < 0 || vk >= static_cast<int>(sizeof(keys) / sizeof(keys[0])))
                return "[-]";
            return keys[vk];
        }

        struct KeybindEntry {
            const char* label;
            const char* enabledKey;
            const char* bindKey;
            const char* alwaysOnKey;
        };

        void CollectActiveKeybinds(CConfig* cfg, std::vector<std::string>& out) {
            static const KeybindEntry entries[] = {
                { "Legit aimbot", "legit_aimbot", "legit_aimbot_key", "legit_aimbot_always_on" },
                { "Legit triggerbot", "legit_triggerbot", "legit_triggerbot_key", "legit_triggerbot_always_on" },
                { "Player ESP", nullptr, "visuals_player_esp_activation_type_key", nullptr },
                { "Third person", "misc_third_person", "misc_third_person_key", nullptr },
                { "View glitch", "misc_tp_glitch", "misc_tp_glitch_key", nullptr },
                { "Walkbot", "misc_walkbot", "misc_walkbot_key", nullptr },
            };

            for (const auto& entry : entries) {
                if (entry.enabledKey && !cfg->b[entry.enabledKey])
                    continue;

                if (entry.alwaysOnKey && cfg->b[entry.alwaysOnKey]) {
                    out.push_back(std::string(entry.label) + "  always on");
                    continue;
                }

                const int style = cfg->i[std::string(entry.bindKey) + "style"];
                if (style == 0) {
                    out.push_back(std::string(entry.label) + "  always on");
                    continue;
                }

                const int vk = cfg->i[entry.bindKey];
                const bool active = cfg->IsBindActive(entry.bindKey);
                out.push_back(std::string(entry.label) + "  " + GetKeyName(vk) + (active ? "  on" : "  off"));
            }
        }
    }

    void DrawKeybindList(int screenWidth, int screenHeight) {
        (void)screenWidth;
        (void)screenHeight;

        CConfig* cfg = CConfig::get();
        if (!cfg->b["misc_keybind_list"])
            return;

        std::vector<std::string> binds;
        CollectActiveKeybinds(cfg, binds);
        if (binds.empty())
            return;

        static bool wasMenuOpen = false;
        const bool menuOpen = CMenu::get()->IsMenuOpened();
        const ImVec2 savedPos(cfg->f["misc_keybind_list_x"], cfg->f["misc_keybind_list_y"]);

        if (!menuOpen || !wasMenuOpen)
            ImGui::SetNextWindowPos(savedPos, ImGuiCond_Always);

        wasMenuOpen = menuOpen;

        ImGui::SetNextWindowBgAlpha(0.88f);

        ImGuiWindowFlags flags =
            ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_AlwaysAutoResize |
            ImGuiWindowFlags_NoSavedSettings;

        if (!CMenu::get()->IsMenuOpened())
            flags |= ImGuiWindowFlags_NoMove;

        if (ImGui::Begin("##KeybindListOverlay", nullptr, flags)) {
            if (menuOpen) {
                ImGui::TextColored(ImVec4(0.64f, 0.83f, 0.12f, 1.f), "Keybinds");
                ImGui::SameLine();
                ImGui::TextDisabled("(drag to move)");
            } else {
                ImGui::TextColored(ImVec4(0.64f, 0.83f, 0.12f, 1.f), "Keybinds");
            }

            ImGui::Separator();

            for (const auto& line : binds)
                ImGui::TextUnformatted(line.c_str());

            const ImVec2 pos = ImGui::GetWindowPos();
            cfg->f["misc_keybind_list_x"] = pos.x;
            cfg->f["misc_keybind_list_y"] = pos.y;
        }
        ImGui::End();
    }
}
