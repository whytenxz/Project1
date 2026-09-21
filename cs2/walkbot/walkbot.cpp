#include "walkbot.h"
#include "nav/nav_file.h"
#include "../memory.h"
#include "../math.h"
#include "../offsets.h"
#include "../game_state.h"
#include "../../Backend/Config/Config.h"
#include "../../nerv/main.hpp"
#include "../../nerv/valve/interfaces/vtables/i_csgo_input.hpp"

#include <ShlObj.h>
#include <Windows.h>

#include <chrono>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#pragma comment(lib, "shell32.lib")

namespace walkbot {
    namespace {
        constexpr float kPi = 3.14159265f;
        constexpr float kMoveSpeed = 450.f;

        struct QAngle {
            float pitch, yaw, roll;
        };

        std::string g_mapName;
        std::unique_ptr<nav_mesh::nav_file> g_navFile;
        std::vector<nav_mesh::vec3_t> g_path;
        size_t g_pathIndex = 0;
        nav_mesh::nav_area* g_currentTarget = nullptr;
        std::chrono::steady_clock::time_point g_areaStartTime{};
        int g_lastAreaIndex = -1;
        bool g_navReady = false;

        nav_mesh::vec3_t ToNavVec3(const Vector3& v) {
            return { v.x, v.y, v.z };
        }

        Vector3 NavToVec3(const nav_mesh::vec3_t& v) {
            return { v.x, v.y, v.z };
        }

        float Dist3D(const nav_mesh::vec3_t& a, const nav_mesh::vec3_t& b) {
            const float dx = a.x - b.x;
            const float dy = a.y - b.y;
            const float dz = a.z - b.z;
            return std::sqrt(dx * dx + dy * dy + dz * dz);
        }

        float DistFlat(const Vector3& a, const Vector3& b) {
            const float dx = a.x - b.x;
            const float dy = a.y - b.y;
            return std::sqrt(dx * dx + dy * dy);
        }

        Vector3 LerpVec(const Vector3& a, const Vector3& b, float t) {
            return { a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t };
        }

        float NormalizeYaw(float yaw) {
            while (yaw > 180.f) yaw -= 360.f;
            while (yaw < -180.f) yaw += 360.f;
            return yaw;
        }

        float ClampPitch(float pitch) {
            if (pitch > 89.f) return 89.f;
            if (pitch < -89.f) return -89.f;
            return pitch;
        }

        void NormalizeAngles(QAngle& angles) {
            angles.pitch = ClampPitch(angles.pitch);
            angles.yaw = NormalizeYaw(angles.yaw);
            angles.roll = 0.f;
        }

        QAngle CalcAngle(const Vector3& from, const Vector3& to) {
            const Vector3 delta{ to.x - from.x, to.y - from.y, to.z - from.z };
            const float hyp = std::sqrt(delta.x * delta.x + delta.y * delta.y);

            QAngle angles{};
            angles.pitch = -std::atan2f(delta.z, hyp) * (180.f / kPi);
            angles.yaw = std::atan2f(delta.y, delta.x) * (180.f / kPi);
            return angles;
        }

        Vector3 GetEyePosition(uintptr_t pawn) {
            const Vector3 origin = memory::Read<Vector3>(pawn + schema::C_BaseEntity::m_vOldOrigin);
            const Vector3 viewOffset = memory::Read<Vector3>(pawn + schema::C_BaseModelEntity::m_vecViewOffset);
            return { origin.x + viewOffset.x, origin.y + viewOffset.y, origin.z + viewOffset.z };
        }

        std::string GetDefaultNavDirectory() {
            char base[MAX_PATH]{};
            if (FAILED(SHGetFolderPathA(nullptr, CSIDL_PERSONAL, nullptr, SHGFP_TYPE_CURRENT, base)))
                GetCurrentDirectoryA(MAX_PATH, base);

            std::string path = std::string(base) + "\\gamesense-cs2\\nav";
            std::error_code ec;
            std::filesystem::create_directories(path, ec);
            return path;
        }

        std::string GetNavDirectory(CConfig* cfg) {
            const char* custom = cfg->s["misc_walkbot_nav_path"];
            if (custom && custom[0] != '\0')
                return custom;
            return GetDefaultNavDirectory();
        }

        std::string GetCurrentMapName() {
            const uintptr_t client = memory::GetModuleBase("client.dll");
            if (!client)
                return {};

            const uintptr_t globalVars = memory::Read<uintptr_t>(client + offsets::client_dll::dwGlobalVars);
            if (!globalVars)
                return {};

            const uintptr_t mapNamePtr = memory::Read<uintptr_t>(globalVars + offsets::global_vars::map_name);
            if (!mapNamePtr)
                return {};

            return memory::ReadString(mapNamePtr, 64);
        }

        nav_mesh::nav_area* GetAreaNearPosition(const Vector3& origin) {
            if (!g_navFile)
                return nullptr;

            const nav_mesh::vec3_t position = ToNavVec3(origin);
            nav_mesh::nav_area* closest = nullptr;
            float closestDistSq = FLT_MAX;

            for (auto& area : g_navFile->get_areas()) {
                const nav_mesh::vec3_t& center = area.get_center();
                const float dx = center.x - position.x;
                const float dy = center.y - position.y;
                const float dz = center.z - position.z;
                const float distSq = dx * dx + dy * dy + dz * dz;

                if (distSq < closestDistSq) {
                    closestDistSq = distSq;
                    closest = &area;
                }
            }

            return closest;
        }

        bool TryLoadNavForMap(const std::string& mapName, CConfig* cfg) {
            if (mapName.empty() || mapName == "<empty>")
                return false;

            const std::string navPath = GetNavDirectory(cfg) + "\\" + mapName + ".nav";
            if (!std::filesystem::exists(navPath))
                return false;

            try {
                g_navFile = std::make_unique<nav_mesh::nav_file>(navPath);
                g_mapName = mapName;
                g_path.clear();
                g_pathIndex = 0;
                g_lastAreaIndex = -1;
                g_currentTarget = nullptr;
                g_navReady = true;
                return true;
            } catch (...) {
                g_navFile.reset();
                g_navReady = false;
                return false;
            }
        }

        bool LevelCheck(CConfig* cfg) {
            const std::string mapName = GetCurrentMapName();
            if (mapName.empty())
                return false;

            if (g_mapName != mapName)
                return TryLoadNavForMap(mapName, cfg);

            return g_navReady && g_navFile != nullptr;
        }

        bool GetFurthestEnemyOrigin(uintptr_t localPawn, Vector3& outOrigin) {
            const uintptr_t client = memory::GetModuleBase("client.dll");
            const uintptr_t entityList = memory::Read<uintptr_t>(client + offsets::client_dll::dwEntityList);
            if (!entityList || !localPawn)
                return false;

            const int localTeam = memory::Read<uint8_t>(localPawn + schema::C_BaseEntity::m_iTeamNum);
            const Vector3 localOrigin = memory::Read<Vector3>(localPawn + schema::C_BaseEntity::m_vOldOrigin);

            float bestDistance = 0.f;
            bool found = false;

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

                if (memory::Read<uint8_t>(pawn + schema::C_BaseEntity::m_lifeState) != 0)
                    continue;

                if (memory::Read<int>(pawn + schema::C_BaseEntity::m_iHealth) <= 0)
                    continue;

                if (memory::Read<uint8_t>(pawn + schema::C_BaseEntity::m_iTeamNum) == localTeam)
                    continue;

                const Vector3 origin = memory::Read<Vector3>(pawn + schema::C_BaseEntity::m_vOldOrigin);
                const float distance = DistFlat(localOrigin, origin);
                if (distance > bestDistance) {
                    bestDistance = distance;
                    outOrigin = origin;
                    found = true;
                }
            }

            return found;
        }

        nav_mesh::nav_area* GetAreaNearEnemies(uintptr_t localPawn) {
            Vector3 enemyOrigin{};
            if (GetFurthestEnemyOrigin(localPawn, enemyOrigin)) {
                nav_mesh::nav_area* area = GetAreaNearPosition(enemyOrigin);
                if (area)
                    return area;
            }

            if (!g_navFile || g_navFile->get_areas().empty())
                return nullptr;

            const int randomIdx = rand() % static_cast<int>(g_navFile->get_areas().size());
            return &g_navFile->get_areas()[static_cast<size_t>(randomIdx)];
        }

        void ClearMovement(c_user_cmd* cmd) {
            if (!cmd)
                return;

            if (auto* base = cmd->get_base_cmd()) {
                base->set_forwardmove(0.f);
                base->set_leftmove(0.f);
            }

            auto& buttons = cmd->m_button_state;
            buttons.m_button_state &= ~static_cast<uint64_t>(IN_FORWARD | IN_BACK | IN_MOVELEFT | IN_MOVERIGHT | IN_JUMP);
            buttons.m_button_state2 &= ~static_cast<uint64_t>(IN_FORWARD | IN_BACK | IN_MOVELEFT | IN_MOVERIGHT | IN_JUMP);
            buttons.m_button_state3 &= ~static_cast<uint64_t>(IN_FORWARD | IN_BACK | IN_MOVELEFT | IN_MOVERIGHT | IN_JUMP);
        }

        void ApplyMovement(c_user_cmd* cmd, float yawDiff) {
            if (!cmd)
                return;

            const float rad = yawDiff * (kPi / 180.f);
            const float forward = std::cos(rad) * kMoveSpeed;
            const float side = std::sin(rad) * kMoveSpeed;

            if (auto* base = cmd->get_base_cmd()) {
                base->set_forwardmove(forward);
                base->set_leftmove(side);
            }

            auto& buttons = cmd->m_button_state;
            buttons.m_button_state &= ~static_cast<uint64_t>(IN_FORWARD | IN_BACK | IN_MOVELEFT | IN_MOVERIGHT);
            buttons.m_button_state2 &= ~static_cast<uint64_t>(IN_FORWARD | IN_BACK | IN_MOVELEFT | IN_MOVERIGHT);

            if (forward > 1.f)
                buttons.m_button_state |= IN_FORWARD;
            else if (forward < -1.f)
                buttons.m_button_state |= IN_BACK;

            if (side > 1.f)
                buttons.m_button_state |= IN_MOVERIGHT;
            else if (side < -1.f)
                buttons.m_button_state |= IN_MOVELEFT;
        }

        void SetJump(c_user_cmd* cmd, bool jump) {
            if (!cmd)
                return;

            auto& buttons = cmd->m_button_state;
            if (jump) {
                buttons.m_button_state |= IN_JUMP;
                buttons.m_button_state2 |= IN_JUMP;
            } else {
                buttons.m_button_state &= ~static_cast<uint64_t>(IN_JUMP);
                buttons.m_button_state2 &= ~static_cast<uint64_t>(IN_JUMP);
                buttons.m_button_state3 &= ~static_cast<uint64_t>(IN_JUMP);
            }
        }

        QAngle ReadViewAngles(uintptr_t client) {
            return memory::Read<QAngle>(client + offsets::client_dll::dwViewAngles);
        }

        void SmoothAim(c_user_cmd* cmd, i_csgo_input* input, const Vector3& eyePos, const Vector3& target, float smooth) {
            if (!cmd || !input)
                return;

            const uintptr_t client = memory::GetModuleBase("client.dll");
            QAngle current = ReadViewAngles(client);
            QAngle desired = CalcAngle(eyePos, target);

            if (smooth < 0.01f)
                smooth = 0.01f;
            if (smooth > 1.f)
                smooth = 1.f;

            QAngle next{
                current.pitch + (desired.pitch - current.pitch) * smooth,
                current.yaw + (desired.yaw - current.yaw) * smooth,
                0.f
            };
            NormalizeAngles(next);

            if (auto* base = cmd->get_base_cmd()) {
                auto* ang = base->mutable_viewangles();
                if (ang) {
                    ang->set_x(next.pitch);
                    ang->set_y(next.yaw);
                    ang->set_z(0.f);
                }
            }

            vec3_t viewAngles{ next.pitch, next.yaw, 0.f };
            input->set_view_angles(viewAngles);
            memory::Write<QAngle>(client + offsets::client_dll::dwViewAngles, next);
        }

        bool NeedsNewPath() {
            return g_path.empty();
        }

        void BuildPath(uintptr_t localPawn) {
            g_currentTarget = GetAreaNearEnemies(localPawn);
            nav_mesh::nav_area* startArea = GetAreaNearPosition(memory::Read<Vector3>(localPawn + schema::C_BaseEntity::m_vOldOrigin));
            if (!g_currentTarget || !startArea || !g_navFile)
                return;

            try {
                g_path = g_navFile->find_path(
                    nav_mesh::vec3_t(startArea->get_center()),
                    nav_mesh::vec3_t(g_currentTarget->get_center()));
                g_pathIndex = 0;
                g_lastAreaIndex = -1;
            } catch (...) {
                g_path.clear();
                g_pathIndex = 0;
            }
        }

        void FollowPath(c_user_cmd* cmd, i_csgo_input* input, uintptr_t localPawn) {
            if (g_path.empty() || g_pathIndex >= g_path.size()) {
                g_path.clear();
                g_pathIndex = 0;
                ClearMovement(cmd);
                return;
            }

            const Vector3 localOrigin = memory::Read<Vector3>(localPawn + schema::C_BaseEntity::m_vOldOrigin);
            const Vector3 eyePos = GetEyePosition(localPawn);
            const nav_mesh::vec3_t& targetPoint = g_path[g_pathIndex];

            if (static_cast<int>(g_pathIndex) != g_lastAreaIndex) {
                g_areaStartTime = std::chrono::steady_clock::now();
                g_lastAreaIndex = static_cast<int>(g_pathIndex);
            }

            const auto now = std::chrono::steady_clock::now();
            const auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - g_areaStartTime).count();

            Vector3 aimTarget = NavToVec3(targetPoint);
            if (g_pathIndex + 1 < g_path.size())
                aimTarget = LerpVec(aimTarget, NavToVec3(g_path[g_pathIndex + 1]), 0.3f);

            aimTarget.z = eyePos.z;

            SmoothAim(cmd, input, eyePos, aimTarget, 0.35f);

            const float distanceFlat = DistFlat(localOrigin, NavToVec3(targetPoint));
            const float verticalDiff = aimTarget.z - localOrigin.z;

            const QAngle current = ReadViewAngles(memory::GetModuleBase("client.dll"));
            const QAngle desired = CalcAngle(eyePos, aimTarget);
            float yawDiff = NormalizeYaw(desired.yaw - current.yaw);

            bool shouldJump = false;
            if (distanceFlat > 40.f && verticalDiff < -14.5f && elapsedMs >= 1400 && elapsedMs < 5000)
                shouldJump = true;
            else if (verticalDiff > 35.f && distanceFlat < 80.f && elapsedMs >= 1400 && elapsedMs < 5000)
                shouldJump = true;

            if (elapsedMs >= 3800)
                shouldJump = true;

            SetJump(cmd, shouldJump);

            if (elapsedMs >= 6000) {
                g_path.clear();
                g_pathIndex = 0;
                g_lastAreaIndex = -1;
                ClearMovement(cmd);
                return;
            }

            if (distanceFlat < 55.f) {
                ++g_pathIndex;
                if (g_pathIndex >= g_path.size()) {
                    g_path.clear();
                    g_pathIndex = 0;
                    ClearMovement(cmd);
                }
            } else {
                ApplyMovement(cmd, yawDiff);
            }
        }
    }

    bool IsActive() {
        CConfig* cfg = CConfig::get();
        if (!cfg->b["misc_walkbot"])
            return false;
        return cfg->IsBindActive("misc_walkbot_key");
    }

    void Process(c_user_cmd* cmd, i_csgo_input* input, uintptr_t localPawn) {
        if (!cmd || !input || !localPawn)
            return;

        CConfig* cfg = CConfig::get();
        if (!IsActive())
            return;

        if (!LevelCheck(cfg)) {
            ClearMovement(cmd);
            return;
        }

        if (NeedsNewPath())
            BuildPath(localPawn);
        else
            FollowPath(cmd, input, localPawn);
    }
}
