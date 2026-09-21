#include "hitmarker.h"
#include "game_state.h"
#include "memory.h"
#include "offsets.h"
#include "../Backend/Config/Config.h"
#include "../Backend/Features/Visuals/EventLogger.h"
#include "../Backend/Utilities/Utilities.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <string>

namespace hitmarker {
    namespace {
        struct PlayerState {
            int health = 0;
            bool valid = false;
        };

        std::array<PlayerState, 65> g_playerStates{};
        int g_lastLocalShots = 0;
        std::chrono::steady_clock::time_point g_lastShotTime{};
        float g_alpha = 0.f;

        void DrawHitmarker(ImDrawList* drawList, int screenWidth, int screenHeight, float alpha) {
            if (alpha <= 0.f)
                return;

            const float cx = screenWidth * 0.5f;
            const float cy = screenHeight * 0.5f;
            const int a = static_cast<int>(std::clamp(alpha, 0.f, 1.f) * 255.f);
            const ImU32 color = IM_COL32(255, 255, 255, a);
            const ImU32 outline = IM_COL32(0, 0, 0, a / 2);
            const float gap = 5.f;
            const float length = 9.f;
            const float thickness = 2.f;

            const ImVec2 segments[4][2] = {
                { ImVec2(cx - gap - length, cy - gap - length), ImVec2(cx - gap, cy - gap) },
                { ImVec2(cx + gap + length, cy - gap - length), ImVec2(cx + gap, cy - gap) },
                { ImVec2(cx - gap - length, cy + gap + length), ImVec2(cx - gap, cy + gap) },
                { ImVec2(cx + gap + length, cy + gap + length), ImVec2(cx + gap, cy + gap) },
            };

            for (const auto& segment : segments) {
                drawList->AddLine(segment[0], segment[1], outline, thickness + 1.f);
                drawList->AddLine(segment[0], segment[1], color, thickness);
            }
        }

        void OnHitDetected(int damage, int remainingHealth, const std::string& name, CConfig* cfg) {
            g_alpha = 1.f;

            if (!cfg->b["misc_hit_logs"])
                return;

            if (remainingHealth <= 0)
                Features::EventLogger->AddLog("Killed %s for %d damage", name.c_str(), damage);
            else
                Features::EventLogger->AddLog("Hit %s for %d damage (%d health remaining)", name.c_str(), damage, remainingHealth);
        }

        void TrackHits(int playerIndex, int health, int team, int localTeam, const std::string& name, bool localRecentlyShot, CConfig* cfg) {
            if (team == localTeam) {
                g_playerStates[playerIndex].valid = false;
                return;
            }

            auto& state = g_playerStates[playerIndex];
            if (state.valid && health < state.health && localRecentlyShot) {
                const int damage = state.health - health;
                const bool wantsHitmarker = cfg->b["misc_hitmarker"];
                const bool wantsLogs = cfg->b["misc_hit_logs"];
                if (wantsHitmarker || wantsLogs)
                    OnHitDetected(damage, health, name, cfg);
            }

            state.health = health;
            state.valid = true;
        }
    }

    void Run(ImDrawList* drawList, int screenWidth, int screenHeight) {
        if (!drawList)
            return;

        CConfig* cfg = CConfig::get();
        const bool wantsHitmarker = cfg->b["misc_hitmarker"];
        const bool wantsLogs = cfg->b["misc_hit_logs"];
        if (!wantsHitmarker && !wantsLogs)
            return;

        const float dt = Misc::Utilities->GetDeltaTime();
        if (g_alpha > 0.f)
            g_alpha = std::max(0.f, g_alpha - dt * 5.f);

        if (wantsHitmarker)
            DrawHitmarker(drawList, screenWidth, screenHeight, g_alpha);

        const uintptr_t client = memory::GetModuleBase("client.dll");
        if (!client || !game_state::IsInMatch())
            return;

        uintptr_t entityList = 0;
        uintptr_t localControllerIgnored = 0;
        uintptr_t localPawn = 0;
        if (!game_state::GetClientPointers(client, entityList, localControllerIgnored, localPawn))
            return;

        const int localShots = memory::Read<int>(localPawn + schema::C_CSPlayerPawn::m_iShotsFired);
        if (localShots > g_lastLocalShots)
            g_lastShotTime = std::chrono::steady_clock::now();
        g_lastLocalShots = localShots;

        const auto now = std::chrono::steady_clock::now();
        const bool localRecentlyShot =
            std::chrono::duration_cast<std::chrono::milliseconds>(now - g_lastShotTime).count() < 400;

        const int localTeam = memory::Read<uint8_t>(localPawn + schema::C_BaseEntity::m_iTeamNum);

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
            std::string name = memory::ReadString(controller + schema::CBasePlayerController::m_iszPlayerName);
            if (name.empty())
                name = "player";

            TrackHits(i, health, team, localTeam, name, localRecentlyShot, cfg);
        }
    }
}
