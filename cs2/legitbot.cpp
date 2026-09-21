#include "legitbot.h"
#include "memory.h"
#include "math.h"
#include "offsets.h"
#include "weapons.h"
#include "../Backend/Config/Config.h"

#include <Windows.h>
#include <chrono>
#include <cmath>

namespace legitbot {
    namespace {
        struct QAngle {
            float pitch, yaw, roll;
        };

        constexpr float kPi = 3.14159265f;
        constexpr std::ptrdiff_t kCameraServices = 0x1238;
        constexpr std::ptrdiff_t kViewPunchAngle = 0x48;
        constexpr std::ptrdiff_t kSpottedState = 0x1C38;
        constexpr std::ptrdiff_t kSpotted = 0x8;
        constexpr float kSmokeRadius = 125.f;
        constexpr float kSmokeHeight = 96.f;

        struct LegitAimSettings {
            float fov = 8.f;
            float smooth = 8.f;
            int hitbox = 0;
            int dynamicMode = 0;
        };

        LegitAimSettings ReadTabSettings(CConfig* cfg, int tab) {
            LegitAimSettings settings{};
            const std::string prefix = "legit_w" + std::to_string(tab);

            settings.fov = cfg->f[prefix + "_fov"];
            settings.smooth = cfg->f[prefix + "_smooth"];
            settings.hitbox = cfg->i[prefix + "_hitbox"] < 0 ? 0 : (cfg->i[prefix + "_hitbox"] > 5 ? 5 : cfg->i[prefix + "_hitbox"]);
            settings.dynamicMode = cfg->i[prefix + "_dynamic_mode"] < 0 ? 0 : (cfg->i[prefix + "_dynamic_mode"] > 2 ? 2 : cfg->i[prefix + "_dynamic_mode"]);

            if (settings.fov <= 0.f) {
                settings.fov = cfg->f["legit_aimbot_fov"] > 0.f ? cfg->f["legit_aimbot_fov"] : 8.f;
                settings.smooth = cfg->f["legit_aimbot_smooth"] > 0.f ? cfg->f["legit_aimbot_smooth"] : 8.f;
                settings.hitbox = cfg->i["legit_aimbot_hitbox"] < 0 ? 0 : (cfg->i["legit_aimbot_hitbox"] > 5 ? 5 : cfg->i["legit_aimbot_hitbox"]);
                settings.dynamicMode = cfg->i["legit_aimbot_dynamic_mode"] < 0 ? 0 : (cfg->i["legit_aimbot_dynamic_mode"] > 2 ? 2 : cfg->i["legit_aimbot_dynamic_mode"]);
            }

            return settings;
        }

        LegitAimSettings GetLegitAimSettings(CConfig* cfg, int weaponGroup) {
            for (int i = 0; i <= 5; ++i) {
                if (cfg->b["legit_w" + std::to_string(i) + "_globalize"])
                    return ReadTabSettings(cfg, i);
            }

            const int tab = (weaponGroup >= 0 && weaponGroup <= 5) ? weaponGroup : 0;
            return ReadTabSettings(cfg, tab);
        }

        bool IsPointInSmoke(const Vector3& point, uintptr_t entityList) {
            const int highest = memory::GetHighestEntityIndex(entityList);
            for (int i = 64; i <= highest; ++i) {
                const uintptr_t entity = memory::GetEntityByIndex(entityList, i);
                if (!entity)
                    continue;

                if (!memory::Read<bool>(entity + schema::C_SmokeGrenadeProjectile::m_bDidSmokeEffect))
                    continue;

                if (memory::Read<int>(entity + schema::C_SmokeGrenadeProjectile::m_nSmokeEffectTickBegin) <= 0)
                    continue;

                const Vector3 smokePos = memory::Read<Vector3>(entity + schema::C_SmokeGrenadeProjectile::m_vSmokeDetonationPos);
                if (!std::isfinite(smokePos.x) || !std::isfinite(smokePos.y) || !std::isfinite(smokePos.z))
                    continue;

                const float dx = point.x - smokePos.x;
                const float dy = point.y - smokePos.y;
                const float dz = point.z - smokePos.z;
                if ((dx * dx + dy * dy) > (kSmokeRadius * kSmokeRadius))
                    continue;
                if (std::fabs(dz) > kSmokeHeight)
                    continue;

                return true;
            }

            return false;
        }

        bool IsBlockedBySmoke(const Vector3& point, uintptr_t entityList, bool enabled) {
            if (!enabled)
                return false;
            return IsPointInSmoke(point, entityList);
        }

        Vector3 GetEyePosition(uintptr_t pawn) {
            const Vector3 origin = memory::Read<Vector3>(pawn + schema::C_BaseEntity::m_vOldOrigin);
            const Vector3 viewOffset = memory::Read<Vector3>(pawn + schema::C_BaseModelEntity::m_vecViewOffset);
            return origin + viewOffset;
        }

        Vector3 GetAimPoint(uintptr_t pawn, int hitbox) {
            const Vector3 origin = memory::Read<Vector3>(pawn + schema::C_BaseEntity::m_vOldOrigin);
            const float eyeHeight = memory::Read<Vector3>(pawn + schema::C_BaseModelEntity::m_vecViewOffset).z;

            float offsetFromEye = -1.f;
            switch (hitbox) {
            case 1: offsetFromEye = -8.f; break;   // neck
            case 2: offsetFromEye = -18.f; break;  // chest
            case 3: offsetFromEye = -28.f; break;  // stomach
            case 4: offsetFromEye = -38.f; break;  // pelvis
            default: offsetFromEye = -1.f; break;  // head
            }

            return Vector3{ origin.x, origin.y, origin.z + eyeHeight + offsetFromEye };
        }

        float ClampPitch(float pitch) {
            if (pitch > 89.f) return 89.f;
            if (pitch < -89.f) return -89.f;
            return pitch;
        }

        void NormalizeAngles(QAngle& angles) {
            while (angles.yaw > 180.f) angles.yaw -= 360.f;
            while (angles.yaw < -180.f) angles.yaw += 360.f;
            angles.pitch = ClampPitch(angles.pitch);
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

        QAngle GetAngularDifference(const QAngle& current, const Vector3& target, const Vector3& eye) {
            QAngle delta = CalcAngle(eye, target);
            delta.pitch -= current.pitch;
            delta.yaw -= current.yaw;
            NormalizeAngles(delta);
            return delta;
        }

        float GetFovDistance(const QAngle& current, const Vector3& target, const Vector3& eye) {
            const QAngle delta = GetAngularDifference(current, target, eye);
            return std::sqrt(delta.pitch * delta.pitch + delta.yaw * delta.yaw);
        }

        QAngle GetRecoil(uintptr_t localPawn) {
            const int shots = memory::Read<int>(localPawn + schema::C_CSPlayerPawn::m_iShotsFired);
            if (shots < 1)
                return {};

            const uintptr_t cameraServices = memory::Read<uintptr_t>(localPawn + kCameraServices);
            if (!cameraServices)
                return {};

            const Vector3 punch = memory::Read<Vector3>(cameraServices + kViewPunchAngle);
            return QAngle{ punch.x * 2.f, punch.y * 2.f, 0.f };
        }

        bool IsVisible(uintptr_t pawn) {
            if (!pawn)
                return false;
            return memory::Read<bool>(pawn + kSpottedState + kSpotted);
        }

        float GetWorldDistance(const Vector3& from, const Vector3& to) {
            const float dx = to.x - from.x;
            const float dy = to.y - from.y;
            const float dz = to.z - from.z;
            return std::sqrt(dx * dx + dy * dy + dz * dz);
        }

        bool IsValidTargetPawn(uintptr_t entityList, uintptr_t localPawn, int localTeam, int index, uintptr_t& outPawn, Vector3& outOrigin) {
            const uintptr_t controller = memory::GetEntityByIndex(entityList, index);
            if (!controller)
                return false;

            if (!memory::Read<bool>(controller + schema::CCSPlayerController::m_bPawnIsAlive))
                return false;

            const uint32_t pawnHandle = memory::Read<uint32_t>(controller + schema::CCSPlayerController::m_hPlayerPawn);
            const uintptr_t pawn = memory::ResolveHandle(entityList, pawnHandle);
            if (!pawn || pawn == localPawn)
                return false;

            if (memory::Read<uint8_t>(pawn + schema::C_BaseEntity::m_lifeState) != 0)
                return false;

            if (memory::Read<int>(pawn + schema::C_BaseEntity::m_iHealth) <= 0)
                return false;

            if (memory::Read<uint8_t>(pawn + schema::C_BaseEntity::m_iTeamNum) == localTeam)
                return false;

            const Vector3 origin = memory::Read<Vector3>(pawn + schema::C_BaseEntity::m_vOldOrigin);
            if (!std::isfinite(origin.x) || !std::isfinite(origin.y) || !std::isfinite(origin.z))
                return false;

            outPawn = pawn;
            outOrigin = origin;
            return true;
        }

        bool GetAimPointForHitbox(uintptr_t pawn, int hitbox, const Vector3& eyePos, const QAngle& viewAngles, float maxFov, Vector3& outAimPoint, float& outFov) {
            outAimPoint = GetAimPoint(pawn, hitbox);
            outFov = GetFovDistance(viewAngles, outAimPoint, eyePos);
            return outFov <= maxFov;
        }

        bool GetDynamicAimPoint(uintptr_t pawn, int dynamicMode, const Vector3& eyePos, const QAngle& viewAngles, float maxFov, Vector3& outAimPoint, float& outFov) {
            constexpr float kDamage[] = { 4.f, 2.5f, 1.f, 1.f, 0.75f };

            if (dynamicMode == 1) {
                float bestDamage = -1.f;
                float bestFov = maxFov + 1.f;
                Vector3 bestPoint{};
                bool found = false;

                for (int hitbox = 0; hitbox < 5; ++hitbox) {
                    Vector3 point{};
                    float fov = 0.f;
                    if (!GetAimPointForHitbox(pawn, hitbox, eyePos, viewAngles, maxFov, point, fov))
                        continue;

                    if (kDamage[hitbox] > bestDamage || (kDamage[hitbox] == bestDamage && fov < bestFov)) {
                        bestDamage = kDamage[hitbox];
                        bestFov = fov;
                        bestPoint = point;
                        found = true;
                    }
                }

                if (!found)
                    return false;

                outAimPoint = bestPoint;
                outFov = bestFov;
                return true;
            }

            float bestFov = maxFov + 1.f;
            Vector3 bestPoint{};
            bool found = false;

            for (int hitbox = 0; hitbox < 5; ++hitbox) {
                const Vector3 point = GetAimPoint(pawn, hitbox);
                const float fov = GetFovDistance(viewAngles, point, eyePos);
                if (fov > maxFov)
                    continue;

                if (fov < bestFov) {
                    bestFov = fov;
                    bestPoint = point;
                    found = true;
                }
            }

            if (!found)
                return false;

            outAimPoint = bestPoint;
            outFov = bestFov;
            return true;
        }
    }

    namespace {
        void FireShot() {
            INPUT down{};
            down.type = INPUT_MOUSE;
            down.mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
            SendInput(1, &down, sizeof(INPUT));

            INPUT up{};
            up.type = INPUT_MOUSE;
            up.mi.dwFlags = MOUSEEVENTF_LEFTUP;
            SendInput(1, &up, sizeof(INPUT));
        }

        std::chrono::steady_clock::time_point g_lastTriggerShot{};
    }

    void RunTriggerbot() {
        CConfig* cfg = CConfig::get();
        if (!cfg->b["legit_triggerbot"])
            return;

        if (!cfg->b["legit_triggerbot_always_on"] && !cfg->IsBindActive("legit_triggerbot_key"))
            return;

        const uintptr_t client = memory::GetModuleBase("client.dll");
        if (!client)
            return;

        const uintptr_t localPawn = memory::Read<uintptr_t>(client + offsets::client_dll::dwLocalPlayerPawn);
        if (!localPawn)
            return;

        if (memory::Read<uint8_t>(localPawn + schema::C_BaseEntity::m_lifeState) != 0)
            return;

        if (memory::Read<int>(localPawn + schema::C_BaseEntity::m_iHealth) <= 0)
            return;

        const uintptr_t entityList = memory::Read<uintptr_t>(client + offsets::client_dll::dwEntityList);
        if (!entityList)
            return;

        const int crosshairId = memory::Read<int>(localPawn + schema::C_CSPlayerPawn::m_iIDEntIndex);
        if (crosshairId <= 0 || crosshairId > 64)
            return;

        const uintptr_t targetEntity = memory::GetEntityByIndex(entityList, crosshairId);
        if (!targetEntity || targetEntity == localPawn)
            return;

        if (memory::Read<uint8_t>(targetEntity + schema::C_BaseEntity::m_lifeState) != 0)
            return;

        const int health = memory::Read<int>(targetEntity + schema::C_BaseEntity::m_iHealth);
        if (health <= 0)
            return;

        const int localTeam = memory::Read<uint8_t>(localPawn + schema::C_BaseEntity::m_iTeamNum);
        const int targetTeam = memory::Read<uint8_t>(targetEntity + schema::C_BaseEntity::m_iTeamNum);
        if (targetTeam == localTeam)
            return;

        if (cfg->b["legit_triggerbot_visible_only"] && !IsVisible(targetEntity))
            return;

        if (IsBlockedBySmoke(memory::Read<Vector3>(targetEntity + schema::C_BaseEntity::m_vOldOrigin), entityList, cfg->b["legit_smoke_check"]))
            return;

        const int delayMs = (std::max)(cfg->i["legit_triggerbot_delay"], 0);
        const auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - g_lastTriggerShot).count() < delayMs)
            return;

        FireShot();
        g_lastTriggerShot = now;
    }

    void RunAimbot() {
        CConfig* cfg = CConfig::get();
        if (!cfg->b["legit_aimbot"])
            return;

        if (!cfg->b["legit_aimbot_always_on"] && !cfg->IsBindActive("legit_aimbot_key"))
            return;

        const uintptr_t client = memory::GetModuleBase("client.dll");
        if (!client)
            return;

        const uintptr_t localPawn = memory::Read<uintptr_t>(client + offsets::client_dll::dwLocalPlayerPawn);
        if (!localPawn)
            return;

        if (memory::Read<uint8_t>(localPawn + schema::C_BaseEntity::m_lifeState) != 0)
            return;

        if (memory::Read<int>(localPawn + schema::C_BaseEntity::m_iHealth) <= 0)
            return;

        const uintptr_t entityList = memory::Read<uintptr_t>(client + offsets::client_dll::dwEntityList);
        if (!entityList)
            return;

        const int localTeam = memory::Read<uint8_t>(localPawn + schema::C_BaseEntity::m_iTeamNum);
        const uint16_t weaponDef = weapons::GetActiveWeaponDefIndex(entityList, localPawn);
        const int weaponGroup = weapons::GetLegitWeaponGroup(weaponDef);
        const LegitAimSettings aimSettings = GetLegitAimSettings(cfg, weaponGroup);
        const int hitboxSetting = aimSettings.hitbox;
        const int dynamicMode = aimSettings.dynamicMode;
        const bool useDynamic = hitboxSetting == 5;
        const int fixedHitbox = hitboxSetting > 4 ? 0 : hitboxSetting;
        const Vector3 eyePos = GetEyePosition(localPawn);
        const Vector3 localOrigin = memory::Read<Vector3>(localPawn + schema::C_BaseEntity::m_vOldOrigin);

        QAngle viewAngles = memory::Read<QAngle>(client + offsets::client_dll::dwViewAngles);

        const float maxFov = aimSettings.fov;
        float bestFov = maxFov;
        Vector3 bestTarget{};
        bool foundTarget = false;
        float bestDistance = -1.f;

        for (int i = 1; i <= 64; ++i) {
            uintptr_t pawn = 0;
            Vector3 origin{};
            if (!IsValidTargetPawn(entityList, localPawn, localTeam, i, pawn, origin))
                continue;

            if (cfg->b["legit_aimbot_visible_only"] && !IsVisible(pawn))
                continue;

            Vector3 aimPoint{};
            float fov = 0.f;
            bool hasPoint = false;

            if (useDynamic && dynamicMode == 2) {
                hasPoint = GetDynamicAimPoint(pawn, 0, eyePos, viewAngles, maxFov, aimPoint, fov);
            } else if (useDynamic) {
                hasPoint = GetDynamicAimPoint(pawn, dynamicMode, eyePos, viewAngles, maxFov, aimPoint, fov);
            } else {
                hasPoint = GetAimPointForHitbox(pawn, fixedHitbox, eyePos, viewAngles, maxFov, aimPoint, fov);
            }

            if (!hasPoint)
                continue;

            if (IsBlockedBySmoke(aimPoint, entityList, cfg->b["legit_smoke_check"]))
                continue;

            if (useDynamic && dynamicMode == 2) {
                const float distance = GetWorldDistance(localOrigin, origin);
                if (distance > bestDistance) {
                    bestDistance = distance;
                    bestFov = fov;
                    bestTarget = aimPoint;
                    foundTarget = true;
                }
                continue;
            }

            if (fov < bestFov) {
                bestFov = fov;
                bestTarget = aimPoint;
                foundTarget = true;
            }
        }

        if (!foundTarget)
            return;

        QAngle delta = GetAngularDifference(viewAngles, bestTarget, eyePos);

        if (cfg->b["legit_aimbot_rcs"]) {
            const QAngle recoil = GetRecoil(localPawn);
            delta.pitch -= recoil.pitch;
            delta.yaw -= recoil.yaw;
        }

        float smooth = aimSettings.smooth;
        if (smooth < 1.f)
            smooth = 1.f;

        QAngle newAngles{
            viewAngles.pitch + delta.pitch / smooth,
            viewAngles.yaw + delta.yaw / smooth,
            0.f
        };
        NormalizeAngles(newAngles);

        memory::Write<QAngle>(client + offsets::client_dll::dwViewAngles, newAngles);
    }

    void Run() {
        RunTriggerbot();
        RunAimbot();
    }

    void Draw(ImDrawList* drawList, int screenWidth, int screenHeight) {
        if (!drawList || screenWidth <= 0 || screenHeight <= 0)
            return;

        CConfig* cfg = CConfig::get();
        if (!cfg->b["legit_aimbot_fov_circle"])
            return;

        const uintptr_t client = memory::GetModuleBase("client.dll");
        if (!client)
            return;

        const uintptr_t localPawn = memory::Read<uintptr_t>(client + offsets::client_dll::dwLocalPlayerPawn);
        const uintptr_t entityList = memory::Read<uintptr_t>(client + offsets::client_dll::dwEntityList);
        float fov = cfg->f["legit_aimbot_fov"];
        if (localPawn && entityList) {
            const uint16_t weaponDef = weapons::GetActiveWeaponDefIndex(entityList, localPawn);
            fov = GetLegitAimSettings(cfg, weapons::GetLegitWeaponGroup(weaponDef)).fov;
        }

        if (fov <= 0.f)
            return;

        constexpr float kVerticalFov = 73.f;
        const float radius = (screenHeight * 0.5f) * std::tanf(fov * (kPi / 180.f)) / std::tanf((kVerticalFov * 0.5f) * (kPi / 180.f));
        if (radius <= 1.f)
            return;

        const ImVec2 center(screenWidth * 0.5f, screenHeight * 0.5f);
        const int* col = cfg->c["legit_aimbot_fov_circle_color"];
        const ImU32 color = IM_COL32(col[0], col[1], col[2], col[3]);
        drawList->AddCircle(center, radius, color, 64, 1.5f);
    }
}
