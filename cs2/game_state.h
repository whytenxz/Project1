#pragma once
#include "memory.h"
#include "offsets.h"
#include <cstdint>

namespace game_state {
    inline bool IsLikelyPtr(uintptr_t ptr) {
        return ptr >= 0x10000ull && ptr < 0x7FFFFFFFFFFFull;
    }

    inline bool IsEntityListValid(uintptr_t entityList) {
        if (!IsLikelyPtr(entityList))
            return false;

        const uintptr_t chunk = memory::Read<uintptr_t>(entityList + 0x10);
        return IsLikelyPtr(chunk);
    }

    inline bool GetClientPointers(uintptr_t client, uintptr_t& entityList, uintptr_t& localController, uintptr_t& localPawn) {
        if (!client)
            return false;

        entityList = memory::Read<uintptr_t>(client + offsets::client_dll::dwEntityList);
        if (!IsEntityListValid(entityList))
            return false;

        localController = memory::Read<uintptr_t>(client + offsets::client_dll::dwLocalPlayerController);
        localPawn = memory::Read<uintptr_t>(client + offsets::client_dll::dwLocalPlayerPawn);
        return true;
    }

    inline bool IsValidPlayerPawn(uintptr_t pawn) {
        if (!IsLikelyPtr(pawn))
            return false;

        const uint8_t lifeState = memory::Read<uint8_t>(pawn + schema::C_BaseEntity::m_lifeState);
        if (lifeState > 1)
            return false;

        const uint8_t team = memory::Read<uint8_t>(pawn + schema::C_BaseEntity::m_iTeamNum);
        if (team != 2 && team != 3)
            return false;

        const int health = memory::Read<int>(pawn + schema::C_BaseEntity::m_iHealth);
        if (health < 0 || health > 100)
            return false;

        const uintptr_t weaponServices = memory::Read<uintptr_t>(pawn + schema::C_BasePlayerPawn::m_pWeaponServices);
        return IsLikelyPtr(weaponServices);
    }

    inline bool IsValidController(uintptr_t controller) {
        if (!IsLikelyPtr(controller))
            return false;

        const uint64_t steamId = memory::Read<uint64_t>(controller + schema::CBasePlayerController::m_steamID);
        if (steamId != 0 && steamId < 76561197960265728ull)
            return false;

        return true;
    }

    inline bool IsInMatch() {
        const uintptr_t client = memory::GetModuleBase("client.dll");
        uintptr_t entityList = 0;
        uintptr_t localController = 0;
        uintptr_t localPawn = 0;

        if (!GetClientPointers(client, entityList, localController, localPawn))
            return false;

        if (!IsLikelyPtr(localController))
            return false;

        return IsValidPlayerPawn(localPawn);
    }

    inline bool IsSafeForEntityScan() {
        const uintptr_t client = memory::GetModuleBase("client.dll");
        uintptr_t entityList = 0;
        uintptr_t localController = 0;
        uintptr_t localPawn = 0;
        return GetClientPointers(client, entityList, localController, localPawn);
    }
}
