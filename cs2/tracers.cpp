#include "tracers.h"
#include "memory.h"
#include "math.h"
#include "offsets.h"
#include "../Backend/Config/Config.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace tracers {
    namespace {
        constexpr float kPi = 3.14159265358979323846f;
        constexpr float kDeg2Rad = kPi / 180.f;
        constexpr float kTrailLife = 2.5f;
        constexpr float kBulletSpeed = 8000.f;
        constexpr float kRayLength = 8000.f;
        constexpr float kThickness = 2.f;

        struct Trace {
            Vector3 start{};
            Vector3 end{};
            float spawnTime = 0.f;
            float totalDist = 0.f;
        };

        std::vector<Trace> g_traces;
        int g_lastShotsFired = 0;

        float GetTime() {
            return static_cast<float>(GetTickCount64()) / 1000.f;
        }

        void AngleToDir(float pitch, float yaw, Vector3& out) {
            const float cp = std::cosf(pitch * kDeg2Rad);
            const float sp = std::sinf(pitch * kDeg2Rad);
            const float cy = std::cosf(yaw * kDeg2Rad);
            const float sy = std::sinf(yaw * kDeg2Rad);
            out.x = cp * cy;
            out.y = cp * sy;
            out.z = -sp;
        }

        Vector3 LerpPos(const Trace& trace, float frac) {
            return {
                trace.start.x + (trace.end.x - trace.start.x) * frac,
                trace.start.y + (trace.end.y - trace.start.y) * frac,
                trace.start.z + (trace.end.z - trace.start.z) * frac
            };
        }

        void AddTrace(const Vector3& eye, float pitch, float yaw) {
            Vector3 dir{};
            AngleToDir(pitch, yaw, dir);

            Trace trace;
            trace.start = eye;
            trace.end = {
                eye.x + dir.x * kRayLength,
                eye.y + dir.y * kRayLength,
                eye.z + dir.z * kRayLength
            };
            trace.spawnTime = GetTime();

            const float dx = trace.end.x - eye.x;
            const float dy = trace.end.y - eye.y;
            const float dz = trace.end.z - eye.z;
            trace.totalDist = std::sqrtf(dx * dx + dy * dy + dz * dz);

            g_traces.push_back(trace);
        }
    }

    void Update(uintptr_t client) {
        CConfig* cfg = CConfig::get();
        if (!cfg->b["visuals_bullet_tracers"] || !client)
            return;

        const uintptr_t localPawn = memory::Read<uintptr_t>(client + offsets::client_dll::dwLocalPlayerPawn);
        if (!localPawn)
            return;

        const int currentShots = memory::Read<int>(localPawn + schema::C_CSPlayerPawn::m_iShotsFired);
        if (currentShots > g_lastShotsFired && g_lastShotsFired >= 0) {
            const Vector3 origin = memory::Read<Vector3>(localPawn + schema::C_BaseEntity::m_vOldOrigin);
            const Vector3 viewOffset = memory::Read<Vector3>(localPawn + schema::C_BaseModelEntity::m_vecViewOffset);
            const Vector3 eye = origin + viewOffset;

            struct QAngle { float pitch, yaw, roll; };
            const QAngle angles = memory::Read<QAngle>(localPawn + schema::C_CSPlayerPawn::m_angEyeAngles);
            AddTrace(eye, angles.pitch, angles.yaw);
        }

        g_lastShotsFired = currentShots;
    }

    void Draw(ImDrawList* drawList, int screenWidth, int screenHeight) {
        CConfig* cfg = CConfig::get();
        if (!cfg->b["visuals_bullet_tracers"] || !drawList)
            return;

        const uintptr_t client = memory::GetModuleBase("client.dll");
        if (!client)
            return;

        float viewMatrix[16]{};
        for (int i = 0; i < 16; ++i)
            viewMatrix[i] = memory::Read<float>(client + offsets::client_dll::dwViewMatrix + i * sizeof(float));

        const float now = GetTime();
        const float maxAge = kTrailLife + 2.f;

        g_traces.erase(
            std::remove_if(g_traces.begin(), g_traces.end(),
                [now, maxAge](const Trace& trace) { return (now - trace.spawnTime) > maxAge; }),
            g_traces.end());

        for (const auto& trace : g_traces) {
            const float age = now - trace.spawnTime;

            float travelTime = trace.totalDist / kBulletSpeed;
            if (travelTime < 0.01f)
                travelTime = 0.01f;

            float bulletFrac = age / travelTime;
            if (bulletFrac > 1.f)
                bulletFrac = 1.f;

            const float trailAge = age - travelTime;
            float trailAlpha = 1.f;
            if (trailAge > 0.f) {
                trailAlpha = 1.f - (trailAge / kTrailLife);
                if (trailAlpha <= 0.f)
                    continue;
                trailAlpha *= trailAlpha;
            }

            constexpr int segments = 16;
            ImVec2 pts[segments + 1];
            bool ok[segments + 1]{};

            for (int s = 0; s <= segments; ++s) {
                const float segFrac = static_cast<float>(s) / static_cast<float>(segments) * bulletFrac;
                const Vector3 pos = LerpPos(trace, segFrac);
                Vector2 screen{};
                ok[s] = ::WorldToScreen(pos, screen, viewMatrix, screenWidth, screenHeight);
                pts[s] = ImVec2(screen.x, screen.y);
            }

            for (int s = 0; s < segments; ++s) {
                if (!ok[s] || !ok[s + 1])
                    continue;

                const float segFrac = static_cast<float>(s) / static_cast<float>(segments);
                const float brightness = 0.2f + 0.8f * segFrac;
                int alpha = static_cast<int>(trailAlpha * brightness * 220.f);
                if (alpha <= 0)
                    continue;
                if (alpha > 255)
                    alpha = 255;

                drawList->AddLine(pts[s], pts[s + 1], IM_COL32(255, 255, 255, alpha), kThickness);

                int glowAlpha = static_cast<int>(trailAlpha * brightness * 40.f);
                if (glowAlpha > 255)
                    glowAlpha = 255;
                drawList->AddLine(pts[s], pts[s + 1], IM_COL32(180, 200, 255, glowAlpha), kThickness * 3.5f);
            }

            if (bulletFrac < 1.f) {
                const Vector3 headPos = LerpPos(trace, bulletFrac);
                Vector2 headScreen{};
                if (::WorldToScreen(headPos, headScreen, viewMatrix, screenWidth, screenHeight)) {
                    const int ha = static_cast<int>(trailAlpha * 255.f);
                    drawList->AddCircleFilled(ImVec2(headScreen.x, headScreen.y), 5.f, IM_COL32(255, 255, 255, ha));
                    drawList->AddCircleFilled(ImVec2(headScreen.x, headScreen.y), 2.5f, IM_COL32(255, 255, 200, ha));
                }
            }

            if (bulletFrac >= 1.f && trailAlpha > 0.05f) {
                Vector2 impactScreen{};
                if (::WorldToScreen(trace.end, impactScreen, viewMatrix, screenWidth, screenHeight)) {
                    const float sz = 4.f * trailAlpha;
                    drawList->AddCircleFilled(
                        ImVec2(impactScreen.x, impactScreen.y),
                        sz,
                        IM_COL32(255, 200, 100, static_cast<int>(trailAlpha * 200.f)));
                    drawList->AddCircle(
                        ImVec2(impactScreen.x, impactScreen.y),
                        sz * 1.8f,
                        IM_COL32(255, 255, 255, static_cast<int>(trailAlpha * 120.f)),
                        0,
                        1.5f);
                }
            }
        }
    }
}
