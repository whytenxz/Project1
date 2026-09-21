#include "players.h"
#include "game_state.h"
#include "memory.h"
#include "math.h"
#include "offsets.h"
#include "weapons.h"

#include <algorithm>
#include <cmath>

namespace players {
    namespace {
        bool IsValidPawn(uintptr_t pawn) {
            return pawn != 0;
        }

        bool IsValidController(uintptr_t controller) {
            if (!controller)
                return false;

            const uint64_t steamId = memory::Read<uint64_t>(controller + schema::CBasePlayerController::m_steamID);
            if (steamId != 0 && steamId < 76561197960265728ull)
                return false;

            return true;
        }

        std::string ReadSanitizedPlayerName(uintptr_t controller) {
            const uintptr_t namePtr = memory::Read<uintptr_t>(controller + schema::CCSPlayerController::m_sSanitizedPlayerName);
            if (!namePtr)
                return {};

            struct NameBuf { char data[32]; };
            NameBuf buf = memory::Read<NameBuf>(namePtr);
            buf.data[31] = '\0';
            if (!buf.data[0])
                return {};

            return std::string(buf.data);
        }

        std::string ReadPlayerName(uintptr_t controller) {
            if (!IsValidController(controller))
                return {};

            const std::string sanitized = ReadSanitizedPlayerName(controller);
            if (!sanitized.empty())
                return sanitized;

            return memory::ReadString(controller + schema::CBasePlayerController::m_iszPlayerName);
        }

    }

    void Gather(std::vector<PlayerInfo>& out) {
        out.clear();

        const uintptr_t client = memory::GetModuleBase("client.dll");
        uintptr_t entityList = 0;
        uintptr_t localController = 0;
        uintptr_t localPawn = 0;
        if (!game_state::GetClientPointers(client, entityList, localController, localPawn))
            return;
        const int localTeam = localPawn ? memory::Read<uint8_t>(localPawn + schema::C_BaseEntity::m_iTeamNum) : 0;
        const Vector3 localOrigin = localPawn ? memory::Read<Vector3>(localPawn + schema::C_BaseEntity::m_vOldOrigin) : Vector3{};

        for (int i = 1; i <= 64; ++i) {
            const uintptr_t controller = memory::GetEntityByIndex(entityList, i);
            if (!IsValidController(controller))
                continue;

            const std::string name = ReadPlayerName(controller);
            if (name.empty())
                continue;

            PlayerInfo info{};
            info.index = i;
            info.name = name;
            info.steamId64 = memory::Read<uint64_t>(controller + schema::CBasePlayerController::m_steamID);
            info.isLocal = controller == localController;
            info.alive = memory::Read<bool>(controller + schema::CCSPlayerController::m_bPawnIsAlive);
            info.armor = memory::Read<int>(controller + schema::CCSPlayerController::m_iPawnArmor);
            info.hasHelmet = memory::Read<bool>(controller + schema::CCSPlayerController::m_bPawnHasHelmet);
            info.hasDefuser = memory::Read<bool>(controller + schema::CCSPlayerController::m_bPawnHasDefuser);

            const uint32_t pawnHandle = memory::Read<uint32_t>(controller + schema::CCSPlayerController::m_hPlayerPawn);
            const uintptr_t pawn = memory::ResolveHandle(entityList, pawnHandle);

            if (IsValidPawn(pawn)) {
                info.team = memory::Read<uint8_t>(pawn + schema::C_BaseEntity::m_iTeamNum);
                if (info.alive) {
                    info.health = memory::Read<int>(pawn + schema::C_BaseEntity::m_iHealth);
                    if (info.armor <= 0)
                        info.armor = memory::Read<int>(pawn + schema::C_CSPlayerPawn::m_ArmorValue);

                    const uint16_t defIndex = weapons::GetActiveWeaponDefIndex(entityList, pawn);
                    const char* weaponName = weapons::GetName(defIndex);
                    if (weaponName)
                        info.weapon = weaponName;

                    const Vector3 origin = memory::Read<Vector3>(pawn + schema::C_BaseEntity::m_vOldOrigin);
                    info.x = origin.x;
                    info.y = origin.y;
                    info.z = origin.z;
                    info.hasPosition = true;

                    const Vector3 velocity = memory::Read<Vector3>(pawn + schema::C_BaseEntity::m_vecVelocity);
                    info.velX = velocity.x;
                    info.velY = velocity.y;
                    info.velZ = velocity.z;
                    info.hasVelocity = true;

                    const float dx = origin.x - localOrigin.x;
                    const float dy = origin.y - localOrigin.y;
                    const float dz = origin.z - localOrigin.z;
                    info.distance = static_cast<int>(std::sqrt(dx * dx + dy * dy + dz * dz) * 0.0254f);
                }
            }

            if (info.team == 0)
                info.team = localTeam;

            out.push_back(std::move(info));
        }
    }

    void GatherSpectators(std::vector<std::string>& out) {
        out.clear();

        if (!game_state::IsInMatch())
            return;

        const uintptr_t client = memory::GetModuleBase("client.dll");
        uintptr_t entityList = 0;
        uintptr_t localController = 0;
        uintptr_t localPawn = 0;
        if (!game_state::GetClientPointers(client, entityList, localController, localPawn))
            return;

        if (!localController || !localPawn)
            return;

        const uint32_t localHandle = memory::Read<uint32_t>(localController + schema::CCSPlayerController::m_hPlayerPawn);

        for (int i = 1; i <= 64; ++i) {
            const uintptr_t controller = memory::GetEntityByIndex(entityList, i);
            if (!IsValidController(controller) || controller == localController)
                continue;

            if (memory::Read<bool>(controller + schema::CCSPlayerController::m_bPawnIsAlive))
                continue;

            const uint32_t obsPawnHandle = memory::Read<uint32_t>(controller + schema::CCSPlayerController::m_hObserverPawn);
            const uintptr_t obsPawn = memory::ResolveHandle(entityList, obsPawnHandle);
            if (!IsValidPawn(obsPawn) || obsPawn == localPawn)
                continue;

            const uintptr_t observerServices = memory::Read<uintptr_t>(obsPawn + schema::C_BasePlayerPawn::m_pObserverServices);
            if (!observerServices)
                continue;

            const uint32_t targetHandle = memory::Read<uint32_t>(observerServices + schema::CPlayer_ObserverServices::m_hObserverTarget);
            if (!targetHandle || targetHandle != localHandle)
                continue;

            const std::string name = ReadPlayerName(controller);
            if (name.empty())
                continue;

            if (std::find(out.begin(), out.end(), name) == out.end())
                out.push_back(name);
        }
    }
}
