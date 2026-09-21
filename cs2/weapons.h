#pragma once
#include "memory.h"
#include "offsets.h"
#include <cstdint>

namespace weapons {
    inline uintptr_t GetItemView(uintptr_t weaponEntity) {
        if (!weaponEntity)
            return 0;
        return weaponEntity
            + schema::C_EconEntity::m_AttributeManager
            + schema::C_AttributeContainer::m_Item;
    }

    inline uint16_t GetDefIndexFromEntity(uintptr_t weaponEntity) {
        const uintptr_t itemView = GetItemView(weaponEntity);
        if (!itemView)
            return 0;

        return memory::Read<uint16_t>(itemView + schema::C_EconItemView::m_iItemDefinitionIndex);
    }

    inline uintptr_t ResolveWeaponEntity(uintptr_t entityList, uint32_t handle) {
        if (!handle || handle == 0xFFFFFFFF)
            return 0;

        const uintptr_t weapon = memory::ResolveHandle(entityList, handle);
        if (!weapon)
            return 0;

        if (GetDefIndexFromEntity(weapon))
            return weapon;

        return 0;
    }

    inline uintptr_t GetActiveWeaponEntity(uintptr_t entityList, uintptr_t pawn) {
        if (!pawn || !entityList)
            return 0;

        const uintptr_t weaponServices = memory::Read<uintptr_t>(pawn + schema::C_BasePlayerPawn::m_pWeaponServices);
        if (!weaponServices)
            return 0;

        const uint32_t activeHandle = memory::Read<uint32_t>(weaponServices + schema::CPlayer_WeaponServices::m_hActiveWeapon);
        uintptr_t weapon = ResolveWeaponEntity(entityList, activeHandle);
        if (weapon)
            return weapon;

        const uint64_t weaponCount = memory::Read<uint64_t>(weaponServices + schema::CPlayer_WeaponServices::m_hMyWeapons);
        const uintptr_t weaponData = memory::Read<uintptr_t>(weaponServices + schema::CPlayer_WeaponServices::m_hMyWeapons + 8);
        if (weaponCount == 0 || weaponCount > 64 || !weaponData)
            return 0;

        for (uint64_t i = 0; i < weaponCount; ++i) {
            const uint32_t handle = memory::Read<uint32_t>(weaponData + i * sizeof(uint32_t));
            weapon = ResolveWeaponEntity(entityList, handle);
            if (weapon)
                return weapon;
        }

        return 0;
    }

    inline uint16_t GetActiveWeaponDefIndex(uintptr_t entityList, uintptr_t pawn) {
        const uintptr_t weapon = GetActiveWeaponEntity(entityList, pawn);
        if (!weapon)
            return 0;
        return GetDefIndexFromEntity(weapon);
    }
    inline const char* GetName(uint16_t defIndex) {
        switch (defIndex) {
        case 1: return "DEAGLE";
        case 2: return "ELITES";
        case 3: return "FIVESEVEN";
        case 4: return "GLOCK";
        case 7: return "AK-47";
        case 8: return "AUG";
        case 9: return "AWP";
        case 10: return "FAMAS";
        case 11: return "G3SG1";
        case 13: return "GALIL";
        case 14: return "M249";
        case 16: return "M4A4";
        case 17: return "MAC-10";
        case 19: return "P90";
        case 23: return "MP5-SD";
        case 24: return "UMP-45";
        case 25: return "XM1014";
        case 26: return "BIZON";
        case 27: return "MAG-7";
        case 28: return "NEGEV";
        case 29: return "SAWED-OFF";
        case 30: return "TEC-9";
        case 31: return "ZEUS";
        case 32: return "P2000";
        case 33: return "MP7";
        case 34: return "MP9";
        case 35: return "NOVA";
        case 36: return "P250";
        case 38: return "SCAR-20";
        case 39: return "SG 553";
        case 40: return "SSG 08";
        case 42: return "KNIFE";
        case 43: return "FLASH";
        case 44: return "HE";
        case 45: return "SMOKE";
        case 46: return "MOLOTOV";
        case 47: return "DECOY";
        case 48: return "INC";
        case 49: return "C4";
        case 57: return "HEALTH";
        case 59: return "KNIFE";
        case 60: return "M4A1-S";
        case 61: return "USP-S";
        case 63: return "CZ75";
        case 64: return "R8";
        default:
            if (defIndex >= 500) return "KNIFE";
            return nullptr;
        }
    }

    // Legit tab index matches menu icons: G=0, P=1, W=2, d=3, f=4, a=5
    inline int GetLegitWeaponGroup(uint16_t defIndex) {
        switch (defIndex) {
        case 1: case 2: case 3: case 4: case 30: case 32: case 36: case 61: case 63: case 64:
            return 0;
        case 17: case 19: case 23: case 24: case 26: case 33: case 34:
            return 1;
        case 7: case 8: case 10: case 13: case 16: case 39: case 60:
            return 2;
        case 25: case 27: case 29: case 35:
            return 3;
        case 14: case 28:
            return 4;
        case 9: case 11: case 38: case 40:
            return 5;
        default:
            return 0;
        }
    }

    inline const char* GetLegitWeaponGroupName(int group) {
        switch (group) {
        case 0: return "Pistols";
        case 1: return "SMGs";
        case 2: return "Rifles";
        case 3: return "Shotguns";
        case 4: return "Heavy";
        case 5: return "Snipers";
        default: return "Unknown";
        }
    }
}
