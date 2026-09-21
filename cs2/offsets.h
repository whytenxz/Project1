#pragma once
#include <cstddef>
#include <cstdint>

// Generated from https://github.com/a2x/cs2-dumper (build 14165, 2026-06-11)
// Re-dump with cs2-dumper after game updates if ESP stops working.

namespace offsets {
    namespace client_dll {
        constexpr std::ptrdiff_t dwEntityList = 0x24E7680;
        constexpr std::ptrdiff_t dwLocalPlayerController = 0x2320570;
        constexpr std::ptrdiff_t dwLocalPlayerPawn = 0x2341528;
        constexpr std::ptrdiff_t dwViewMatrix = 0x23469C0;
        constexpr std::ptrdiff_t dwGameEntitySystem_highestEntityIndex = 0x2090;
        constexpr std::ptrdiff_t dwGameRules = 0x2340FE8;
        constexpr std::ptrdiff_t dwGlobalVars = 0x20616D0;
        constexpr std::ptrdiff_t dwPlantedC4 = 0x234FE28;
        constexpr std::ptrdiff_t dwViewAngles = 0x2356748;
        constexpr std::ptrdiff_t dwCSGOInput = 0x23560C0;
    }

    namespace signatures {
        constexpr const char* third_person_reset = "48 8B 40 08 44 38 ? 75 10 44 88 ? 01";
        constexpr const char* set_model = "40 53 48 83 EC ? 48 8B D9 4C 8B C2 48 8B 0D ? ? ? ? 48 8D 54 24";
        constexpr const char* resource_precache = "40 53 55 57 48 81 EC 80 00 00 00 48 8B 01 49 8B E8 48 8B FA";
        constexpr const char* generate_primitives = "48 8B C4 48 89 58 08 48 89 50 10 55 56 57 41 54 41 55 41 56 41 57 48 81 EC ? ? ? ?";
        constexpr const char* create_material = "48 89 5C 24 08 48 89 6C 24 10 48 89 74 24 18 48 89 7C 24 20 41 56 48 81 EC 10 01 00 00 48 8B 05 ? ? ? ? 4C 8B F2";
        constexpr const char* set_type_kv3 = "40 53 48 83 EC 30 4C 8B 11 41 B9 ? ? ? ? 49 83 CA 01 0F B6 C2 80 FA 06 48 8B D9 44 0F 45 C8";
        constexpr const char* light_scene_object = "48 89 54 24 10 55 57 41 56 48 83 EC 50";
        constexpr const char* draw_skybox_array = "45 85 C9 0F 8E ? ? ? ? 4C 8B DC 55 41 56 49";
        constexpr const char* create_subtick_move_step = "E8 ? ? ? ? 48 8B D0 48 8D 4F ? E8 ? ? ? ? 48 8B D0";
        constexpr const char* add_subtick_to_rep_field = "E8 ? ? ? ? 48 8B D0 8B 4A ? F3 41 0F 10 46";
        constexpr const char* get_render_fov = "E8 ? ? ? ? F3 0F 11 45 00 48 8B 5C 24 40";
        constexpr const char* render_flashbang_overlay = "85 D2 0F 88 ? ? ? ? 48 89 4C 24";
        constexpr const char* trace_shape = "48 89 5C 24 ? 48 89 4C 24 ? 55 57";
        constexpr const char* trace_manager = "48 8B 0D ? ? ? ? 48 85 C0 74 ? 48 8B 01";
        constexpr const char* trace_filter_vtable = "4C 8D 2D ? ? ? ? 24";
    }

    namespace csgo_input {
        constexpr std::ptrdiff_t block_shot = 0x228;
        constexpr std::ptrdiff_t third_person = 0x229;
        constexpr std::ptrdiff_t button_pressed = 0x240;
        constexpr std::ptrdiff_t mouse_button_pressed = 0x248;
        constexpr std::ptrdiff_t forward_move = 0x260;
        constexpr std::ptrdiff_t left_move = 0x264;
        constexpr std::ptrdiff_t up_move = 0x268;
        constexpr std::ptrdiff_t mouse_delta_x = 0x26C;
        constexpr std::ptrdiff_t mouse_delta_y = 0x270;
        constexpr std::ptrdiff_t view_angles = 0x2D8;
    }

    namespace engine2_dll {
        constexpr std::ptrdiff_t dwWindowWidth = 0x90E5C0;
        constexpr std::ptrdiff_t dwWindowHeight = 0x90E5C4;
    }

    namespace global_vars {
        constexpr std::ptrdiff_t curtime = 0x30;
        constexpr std::ptrdiff_t map_name = 0x180;
    }

    namespace buttons {
        constexpr std::ptrdiff_t jump = 0x2065FA0;
    }
}

namespace schema {
    namespace C_BaseEntity {
        constexpr std::ptrdiff_t m_iHealth = 0x34C;
        constexpr std::ptrdiff_t m_lifeState = 0x354;
        constexpr std::ptrdiff_t m_iTeamNum = 0x3EB;
        constexpr std::ptrdiff_t m_pGameSceneNode = 0x330;
        constexpr std::ptrdiff_t m_flSimulationTime = 0x3B8;
        constexpr std::ptrdiff_t m_hOwnerEntity = 0x520;
        constexpr std::ptrdiff_t m_vOldOrigin = 0x1390; // C_BasePlayerPawn
        constexpr std::ptrdiff_t m_fFlags = 0x3F8;
        constexpr std::ptrdiff_t m_MoveType = 0x525;
        constexpr std::ptrdiff_t m_vecVelocity = 0x430;
    }

    namespace CGameSceneNode {
        constexpr std::ptrdiff_t m_vecAbsOrigin = 0xC8;
    }

    namespace CSkeletonInstance {
        constexpr std::ptrdiff_t m_modelState = 0x150;
    }

    namespace CModelState {
        constexpr std::ptrdiff_t m_boneArray = 0x80;
    }

    namespace C_BasePlayerPawn {
        constexpr std::ptrdiff_t m_pWeaponServices = 0x11E0;
        constexpr std::ptrdiff_t m_pObserverServices = 0x11F8;
        constexpr std::ptrdiff_t m_pCameraServices = 0x1218;
    }

    namespace CEntityInstance {
        constexpr std::ptrdiff_t m_pEntity = 0x10;
    }

    namespace CEntityIdentity {
        constexpr std::ptrdiff_t m_designerName = 0x20;
    }

    namespace C_BaseModelEntity {
        constexpr std::ptrdiff_t m_vecViewOffset = 0xE70;
        constexpr std::ptrdiff_t m_Glow = 0xDD8;
        constexpr std::ptrdiff_t m_ClientOverrideTint = 0xF58;
        constexpr std::ptrdiff_t m_bUseClientOverrideTint = 0xF5C;
    }

    namespace C_EnvSky {
        constexpr std::ptrdiff_t m_vTintColor = 0xFB9;
        constexpr std::ptrdiff_t m_flBrightnessScale = 0xFC4;
        constexpr std::ptrdiff_t m_bEnabled = 0xFDC;
    }

    namespace CPlayer_CameraServices {
        constexpr std::ptrdiff_t m_bOverrideFogColor = 0x1B4;
        constexpr std::ptrdiff_t m_OverrideFogColor = 0x1B9;
        constexpr std::ptrdiff_t m_hActivePostProcessingVolume = 0x1FC;
    }

    namespace C_PostProcessingVolume {
        constexpr std::ptrdiff_t m_flMinExposure = 0x109C;
        constexpr std::ptrdiff_t m_flMaxExposure = 0x10A0;
    }

    namespace CGlowProperty {
        constexpr std::ptrdiff_t m_iGlowType = 0x30;
        constexpr std::ptrdiff_t m_glowColorOverride = 0x40;
        constexpr std::ptrdiff_t m_bGlowing = 0x51;
    }

    namespace C_CSPlayerPawn {
        constexpr std::ptrdiff_t m_flLastSpawnTimeIndex = 0x13DC; // C_CSPlayerPawnBase
        constexpr std::ptrdiff_t m_ArmorValue = 0x1C7C;
        constexpr std::ptrdiff_t m_bIsScoped = 0x1C50;
        constexpr std::ptrdiff_t m_bIsDefusing = 0x1C52;
        constexpr std::ptrdiff_t m_iShotsFired = 0x1C64;
        constexpr std::ptrdiff_t m_angEyeAngles = 0x3320;
        constexpr std::ptrdiff_t m_iIDEntIndex = 0x33FC;
    }

    namespace CPlayer_ObserverServices {
        constexpr std::ptrdiff_t m_iObserverMode = 0x48;
        constexpr std::ptrdiff_t m_hObserverTarget = 0x4C;
        constexpr std::ptrdiff_t m_flObserverChaseDistance = 0x58;
    }

    namespace CPlayer_WeaponServices {
        constexpr std::ptrdiff_t m_hMyWeapons = 0x48;
        constexpr std::ptrdiff_t m_hActiveWeapon = 0x60;
    }

    namespace C_EconEntity {
        constexpr std::ptrdiff_t m_AttributeManager = 0x1180;
    }

    namespace C_AttributeContainer {
        constexpr std::ptrdiff_t m_Item = 0x50;
    }

    namespace C_EconItemView {
        constexpr std::ptrdiff_t m_iItemDefinitionIndex = 0x1BA;
        constexpr std::ptrdiff_t m_bInitialized = 0x1E8;
    }

    namespace CCSPlayerController {
        constexpr std::ptrdiff_t m_hPlayerPawn = 0x90C;
        constexpr std::ptrdiff_t m_hObserverPawn = 0x910;
        constexpr std::ptrdiff_t m_bPawnIsAlive = 0x914;
        constexpr std::ptrdiff_t m_sSanitizedPlayerName = 0x860;
        constexpr std::ptrdiff_t m_iPawnArmor = 0x91C;
        constexpr std::ptrdiff_t m_bPawnHasDefuser = 0x920;
        constexpr std::ptrdiff_t m_bPawnHasHelmet = 0x921;
    }

    namespace CBasePlayerController {
        constexpr std::ptrdiff_t m_iszPlayerName = 0x6F4;
        constexpr std::ptrdiff_t m_steamID = 0x780;
    }

    namespace C_CSGameRulesProxy {
        constexpr std::ptrdiff_t m_pGameRules = 0x600;
    }

    namespace C_CSGameRules {
        constexpr std::ptrdiff_t m_bBombPlanted = 0x8C7;
    }

    namespace C_PlantedC4 {
        constexpr std::ptrdiff_t m_bBombTicking = 0x1160;
        constexpr std::ptrdiff_t m_nBombSite = 0x1164;
        constexpr std::ptrdiff_t m_flC4Blow = 0x1190;
        constexpr std::ptrdiff_t m_bHasExploded = 0x1195;
        constexpr std::ptrdiff_t m_flTimerLength = 0x1198;
        constexpr std::ptrdiff_t m_bBeingDefused = 0x119C;
        constexpr std::ptrdiff_t m_flDefuseCountDown = 0x11B0;
        constexpr std::ptrdiff_t m_bBombDefused = 0x11B4;
    }

    namespace C_SmokeGrenadeProjectile {
        constexpr std::ptrdiff_t m_nSmokeEffectTickBegin = 0x1250;
        constexpr std::ptrdiff_t m_bDidSmokeEffect = 0x1254;
        constexpr std::ptrdiff_t m_vSmokeDetonationPos = 0x1268;
    }
}
