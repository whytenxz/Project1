#include "tp_glitch.h"
#include "game_state.h"
#include "math.h"
#include "memory.h"
#include "offsets.h"
#include "thirdperson.h"
#include "../Backend/Config/Config.h"
#include "../nerv/main.hpp"
#include "imgui/imgui.h"

#include <cmath>
#include <cstring>

namespace tp_glitch {
    namespace {
        constexpr float kPi = 3.14159265358979323846f;
        constexpr float kDeg2Rad = kPi / 180.f;
        constexpr uint64_t kTraceMask = 0x1C1003;
        constexpr float kDefaultChaseDistance = 150.f;

        struct QAngle {
            float pitch, yaw, roll;
        };

        enum RayType_t : uint8_t {
            RAY_TYPE_LINE = 0,
        };

        struct Ray_t {
            union {
                struct {
                    Vector3 m_vStartOffset;
                    float m_flRadius;
                } m_Line;
            };
            RayType_t m_eType = RAY_TYPE_LINE;

            void InitLine(const Vector3& offset = {}) {
                m_Line.m_vStartOffset = offset;
                m_Line.m_flRadius = 0.f;
                m_eType = RAY_TYPE_LINE;
            }
        };

        struct TraceHitboxData_t {
            uint8_t pad01[0x38];
            int m_nHitGroup;
            uint8_t pad02[0x4];
            int m_nHitboxId;
        };

        struct Trace_t {
            void* m_pSurface = nullptr;
            void* m_pHitEntity = nullptr;
            TraceHitboxData_t* m_pHitboxData = nullptr;
            uint8_t pad01[0x38];
            uint32_t m_uContents = 0;
            uint8_t pad02[0x24];
            Vector3 m_vecStartPos{};
            Vector3 m_vecEndPos{};
            Vector3 m_vecNormal{};
            Vector3 m_vecPosition{};
            uint8_t pad03[0x4];
            float m_flFraction = 1.f;
            uint8_t pad04[0x6];
            bool m_bAllSolid = false;
            uint8_t pad05[0x51];
        };

        struct TraceFilter_t {
            uint8_t pad01[0x8];
            int64_t m_uTraceMask = 0;
            int64_t m_v1[2]{};
            int32_t m_arrSkipHandles[4]{};
            int16_t m_arrCollisions[2]{};
            int16_t m_v2 = 0;
            uint8_t m_nLayer = 4;
            uint8_t m_v4 = 0;
            uint8_t m_flags = 0;
        };

        using trace_shape_fn = bool(__fastcall*)(
            void* traceManager,
            Ray_t* ray,
            Vector3* start,
            Vector3* end,
            TraceFilter_t* filter,
            Trace_t* trace);

        trace_shape_fn g_traceShape = nullptr;
        void* g_traceManager = nullptr;
        void* g_traceFilterVtable = nullptr;

        Vector3 g_markerPos{};
        Vector3 g_hitNormal{};
        Vector3 g_viewTargetPos{};
        Vector3 g_savedViewOffset{};
        bool g_hasMarker = false;
        bool g_viewGlitchActive = false;
        bool g_hasSavedViewOffset = false;

        uintptr_t ResolveRelative(uintptr_t address, int rvaOffset, int ripOffset) {
            const int32_t rel = memory::Read<int32_t>(address + rvaOffset);
            return address + ripOffset + rel;
        }

        bool InitializeTrace() {
            if (g_traceShape && g_traceManager)
                return true;

            const char* client = "client.dll";

            if (g_opcodes && g_modules) {
                if (!g_traceShape) {
                    uint8_t* shape = g_opcodes->scan(client, offsets::signatures::trace_shape);
                    if (shape)
                        g_traceShape = reinterpret_cast<trace_shape_fn>(shape);
                }

                if (!g_traceManager) {
                    uint8_t* mgrInsn = g_opcodes->scan(client, offsets::signatures::trace_manager);
                    if (mgrInsn)
                        g_traceManager = reinterpret_cast<void*>(ResolveRelative(reinterpret_cast<uintptr_t>(mgrInsn), 3, 7));
                }

                if (!g_traceFilterVtable) {
                    uint8_t* filterInsn = g_opcodes->scan(client, offsets::signatures::trace_filter_vtable);
                    if (filterInsn)
                        g_traceFilterVtable = reinterpret_cast<void*>(ResolveRelative(reinterpret_cast<uintptr_t>(filterInsn), 3, 7));
                }
            } else {
                if (!g_traceShape) {
                    const uintptr_t shape = memory::FindPattern(client, offsets::signatures::trace_shape);
                    if (shape)
                        g_traceShape = reinterpret_cast<trace_shape_fn>(shape);
                }

                if (!g_traceManager) {
                    const uintptr_t mgrInsn = memory::FindPattern(client, offsets::signatures::trace_manager);
                    if (mgrInsn)
                        g_traceManager = reinterpret_cast<void*>(ResolveRelative(mgrInsn, 3, 7));
                }

                if (!g_traceFilterVtable) {
                    const uintptr_t filterInsn = memory::FindPattern(client, offsets::signatures::trace_filter_vtable);
                    if (filterInsn)
                        g_traceFilterVtable = reinterpret_cast<void*>(ResolveRelative(filterInsn, 3, 7));
                }
            }

            return g_traceShape && g_traceManager;
        }

        void InitTraceFilter(TraceFilter_t& filter, uintptr_t skipPawn) {
            std::memset(&filter, 0, sizeof(filter));
            if (g_traceFilterVtable)
                *reinterpret_cast<void**>(&filter) = g_traceFilterVtable;

            filter.m_uTraceMask = static_cast<int64_t>(kTraceMask);
            filter.m_nLayer = 4;
            filter.m_flags = 2;

            if (skipPawn) {
                const uintptr_t identity = memory::Read<uintptr_t>(skipPawn + schema::CEntityInstance::m_pEntity);
                if (identity) {
                    const int index = memory::Read<int>(identity + 0x10);
                    filter.m_arrSkipHandles[0] = index;
                }
            }
        }

        void AngleToDirection(float pitch, float yaw, Vector3& out) {
            const float cp = std::cosf(pitch * kDeg2Rad);
            const float sp = std::sinf(pitch * kDeg2Rad);
            const float cy = std::cosf(yaw * kDeg2Rad);
            const float sy = std::sinf(yaw * kDeg2Rad);
            out.x = cp * cy;
            out.y = cp * sy;
            out.z = -sp;
        }

        Vector3 GetEyePosition(uintptr_t pawn) {
            const Vector3 origin = memory::Read<Vector3>(pawn + schema::C_BaseEntity::m_vOldOrigin);
            const Vector3 viewOffset = memory::Read<Vector3>(pawn + schema::C_BaseModelEntity::m_vecViewOffset);
            return origin + viewOffset;
        }

        bool TraceLine(uintptr_t skipPawn, const Vector3& start, const Vector3& end, Trace_t& trace) {
            if (!InitializeTrace())
                return false;

            Ray_t ray{};
            ray.InitLine({});

            TraceFilter_t filter{};
            InitTraceFilter(filter, skipPawn);

            Vector3 traceStart = start;
            Vector3 traceEnd = end;
            std::memset(&trace, 0, sizeof(trace));

            return g_traceShape(g_traceManager, &ray, &traceStart, &traceEnd, &filter, &trace);
        }

        bool FindWallTarget(uintptr_t localPawn, float maxDistance, Vector3& outHit, Vector3& outNormal) {
            const Vector3 eye = GetEyePosition(localPawn);
            const QAngle angles = memory::Read<QAngle>(localPawn + schema::C_CSPlayerPawn::m_angEyeAngles);

            Vector3 dir{};
            AngleToDirection(angles.pitch, angles.yaw, dir);

            const Vector3 end{
                eye.x + dir.x * maxDistance,
                eye.y + dir.y * maxDistance,
                eye.z + dir.z * maxDistance
            };

            Trace_t trace{};
            if (TraceLine(localPawn, eye, end, trace) && trace.m_flFraction > 0.f && trace.m_flFraction < 0.99f) {
                outHit = trace.m_vecPosition;
                outNormal = trace.m_vecNormal;

                const float normalLen = std::sqrt(outNormal.x * outNormal.x + outNormal.y * outNormal.y + outNormal.z * outNormal.z);
                if (normalLen > 0.001f) {
                    outNormal.x /= normalLen;
                    outNormal.y /= normalLen;
                    outNormal.z /= normalLen;
                } else {
                    outNormal = { -dir.x, -dir.y, -dir.z };
                }

                return true;
            }

            outHit = {
                eye.x + dir.x * (maxDistance * 0.65f),
                eye.y + dir.y * (maxDistance * 0.65f),
                eye.z + dir.z * (maxDistance * 0.65f)
            };
            outNormal = { -dir.x, -dir.y, -dir.z };
            return true;
        }

        void RestoreViewOffset(uintptr_t localPawn) {
            if (!localPawn || !g_hasSavedViewOffset)
                return;

            memory::Write<Vector3>(localPawn + schema::C_BaseModelEntity::m_vecViewOffset, g_savedViewOffset);
            g_hasSavedViewOffset = false;
        }

        void ApplyViewInWall(uintptr_t localPawn, float viewDepth) {
            const QAngle angles = memory::Read<QAngle>(localPawn + schema::C_CSPlayerPawn::m_angEyeAngles);
            Vector3 dir{};
            AngleToDirection(angles.pitch, angles.yaw, dir);

            if (!g_hasSavedViewOffset) {
                g_savedViewOffset = memory::Read<Vector3>(localPawn + schema::C_BaseModelEntity::m_vecViewOffset);
                g_hasSavedViewOffset = true;
            }

            const Vector3 glitchOffset{
                g_savedViewOffset.x + dir.x * viewDepth,
                g_savedViewOffset.y + dir.y * viewDepth,
                g_savedViewOffset.z + dir.z * viewDepth
            };
            memory::Write<Vector3>(localPawn + schema::C_BaseModelEntity::m_vecViewOffset, glitchOffset);

            thirdperson::SetInputThirdPerson(true);
            thirdperson::SetObserverChaseDistance(localPawn, -viewDepth);
        }

        void RestoreThirdPersonState() {
            CConfig* cfg = CConfig::get();
            const bool thirdPersonActive = cfg->b["misc_third_person"] && cfg->IsBindActive("misc_third_person_key");
            thirdperson::SetInputThirdPerson(thirdPersonActive);

            const uintptr_t client = memory::GetModuleBase("client.dll");
            if (!client)
                return;

            const uintptr_t localPawn = memory::Read<uintptr_t>(client + offsets::client_dll::dwLocalPlayerPawn);
            if (localPawn)
                thirdperson::SetObserverChaseDistance(localPawn, kDefaultChaseDistance);
        }

        bool ReadViewMatrix(float out[16]) {
            const uintptr_t client = memory::GetModuleBase("client.dll");
            if (!client)
                return false;

            for (int i = 0; i < 16; ++i)
                out[i] = memory::Read<float>(client + offsets::client_dll::dwViewMatrix + i * sizeof(float));
            return true;
        }
    }

    bool IsViewGlitchActive() {
        return g_viewGlitchActive;
    }

    bool IsMarkerVisible() {
        CConfig* cfg = CConfig::get();
        return cfg->b["misc_tp_glitch"] && cfg->IsBindActive("misc_tp_glitch_key") && g_hasMarker;
    }

    void Tick(uintptr_t client) {
        g_hasMarker = false;
        g_viewGlitchActive = false;

        CConfig* cfg = CConfig::get();
        if (!cfg->b["misc_tp_glitch"] || !client || !game_state::IsInMatch())
            return;

        const uintptr_t localPawn = memory::Read<uintptr_t>(client + offsets::client_dll::dwLocalPlayerPawn);
        if (!localPawn || memory::Read<int>(localPawn + schema::C_BaseEntity::m_iHealth) <= 0)
            return;

        const float maxDistance = std::clamp(cfg->f["misc_tp_glitch_distance"], 32.f, 256.f);
        const float viewDepth = std::clamp(cfg->f["misc_tp_glitch_through"], 4.f, 64.f);
        const bool bindActive = cfg->IsBindActive("misc_tp_glitch_key");

        Vector3 hit{};
        Vector3 normal{};
        if (!FindWallTarget(localPawn, maxDistance, hit, normal))
            return;

        g_markerPos = hit;
        g_hitNormal = normal;
        g_viewTargetPos = {
            hit.x + normal.x * viewDepth,
            hit.y + normal.y * viewDepth,
            hit.z + normal.z * viewDepth
        };

        if (bindActive) {
            g_hasMarker = true;
            g_viewGlitchActive = true;
        } else if (g_hasSavedViewOffset) {
            RestoreViewOffset(localPawn);
        }
    }

    void ApplyView(uintptr_t client) {
        CConfig* cfg = CConfig::get();
        const bool featureEnabled = cfg->b["misc_tp_glitch"];
        const bool thirdPersonEnabled = cfg->b["misc_third_person"];

        thirdperson::EnsurePatch(featureEnabled || thirdPersonEnabled);

        if (!featureEnabled || !client || !game_state::IsInMatch()) {
            if (g_hasSavedViewOffset) {
                const uintptr_t localPawn = memory::Read<uintptr_t>(client + offsets::client_dll::dwLocalPlayerPawn);
                RestoreViewOffset(localPawn);
            }
            if (!g_viewGlitchActive)
                return;
            RestoreThirdPersonState();
            g_viewGlitchActive = false;
            return;
        }

        if (!g_viewGlitchActive) {
            RestoreThirdPersonState();
            return;
        }

        const uintptr_t localPawn = memory::Read<uintptr_t>(client + offsets::client_dll::dwLocalPlayerPawn);
        if (!localPawn || memory::Read<int>(localPawn + schema::C_BaseEntity::m_iHealth) <= 0) {
            g_viewGlitchActive = false;
            RestoreThirdPersonState();
            return;
        }

        const float viewDepth = std::clamp(cfg->f["misc_tp_glitch_through"], 4.f, 64.f);
        ApplyViewInWall(localPawn, viewDepth);
    }

    void Draw(ImDrawList* drawList, int screenWidth, int screenHeight) {
        if (!drawList || !IsMarkerVisible())
            return;

        float matrix[16]{};
        if (!ReadViewMatrix(matrix))
            return;

        Vector2 screen{};
        if (!WorldToScreen(g_markerPos, screen, matrix, screenWidth, screenHeight))
            return;

        Vector2 destScreen{};
        const bool hasDest = WorldToScreen(g_viewTargetPos, destScreen, matrix, screenWidth, screenHeight);

        const ImU32 accent = IM_COL32(163, 212, 31, 230);
        const ImU32 accentDim = IM_COL32(163, 212, 31, 90);
        const ImVec2 center(screenWidth * 0.5f, screenHeight * 0.5f);
        const ImVec2 marker(screen.x, screen.y);

        drawList->AddLine(center, marker, accentDim, 1.5f);
        drawList->AddCircleFilled(marker, 5.f, accent);
        drawList->AddCircle(marker, 9.f, IM_COL32(255, 255, 255, 210), 24, 2.f);
        drawList->AddCircle(marker, 14.f, accentDim, 24, 1.5f);

        if (hasDest)
            drawList->AddLine(marker, ImVec2(destScreen.x, destScreen.y), IM_COL32(255, 255, 255, 160), 1.5f);

        drawList->AddText(ImVec2(marker.x + 10.f, marker.y - 16.f), IM_COL32(255, 255, 255, 230), "VIEW");
    }
}
