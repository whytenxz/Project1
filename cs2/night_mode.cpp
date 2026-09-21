#include "night_mode.h"
#include "game_state.h"
#include "memory.h"
#include "offsets.h"
#include "../Backend/Config/Config.h"

#include <algorithm>
#include <string>
#include <vector>

namespace night_mode {
    namespace {
        struct ColorRGBA {
            uint8_t r, g, b, a;
        };

        constexpr float kDefaultExposure = 1.f;
        constexpr float kDefaultSkyBrightness = 1.f;
        constexpr int kMaxSkyEntities = 8;

        std::vector<uintptr_t> g_skyEntities;
        bool g_wasEnabled = false;
        bool g_skyScanned = false;

        bool IsValidPtr(uintptr_t ptr) {
            return game_state::IsLikelyPtr(ptr);
        }

        bool HasSceneNode(uintptr_t entity) {
            if (!IsValidPtr(entity))
                return false;

            uintptr_t sceneNode = 0;
            if (!memory::detail::ReadBytes(entity + schema::C_BaseEntity::m_pGameSceneNode, &sceneNode, sizeof(sceneNode)))
                return false;

            return IsValidPtr(sceneNode);
        }

        std::string ReadDesignerName(uintptr_t entity) {
            if (!HasSceneNode(entity))
                return {};

            const uintptr_t identity = memory::Read<uintptr_t>(entity + schema::CEntityInstance::m_pEntity);
            if (!IsValidPtr(identity))
                return {};

            const uintptr_t namePtr = memory::Read<uintptr_t>(identity + schema::CEntityIdentity::m_designerName);
            return memory::ReadString(namePtr, 64);
        }

        bool IsSkyEntity(uintptr_t entity) {
            const std::string name = ReadDesignerName(entity);
            return !name.empty()
                && (name.find("env_sky") != std::string::npos || name.find("sky") != std::string::npos);
        }

        void ResetState() {
            g_skyEntities.clear();
            g_skyScanned = false;
            g_wasEnabled = false;
        }

        void DiscoverSkyEntities(uintptr_t entityList) {
            g_skyEntities.clear();

            if (!entityList)
                return;

            const int highest = memory::GetHighestEntityIndex(entityList);
            for (int i = 0; i <= highest && static_cast<int>(g_skyEntities.size()) < kMaxSkyEntities; ++i) {
                const uintptr_t entity = memory::GetEntityByIndex(entityList, i);
                if (!entity || !IsSkyEntity(entity))
                    continue;

                g_skyEntities.push_back(entity);
            }

            g_skyScanned = true;
        }

        void PruneSkyEntities() {
            g_skyEntities.erase(
                std::remove_if(g_skyEntities.begin(), g_skyEntities.end(),
                    [](uintptr_t entity) { return !HasSceneNode(entity); }),
                g_skyEntities.end());
        }

        bool ApplyPostProcess(uintptr_t entityList, uintptr_t localPawn, float exposure) {
            if (!IsValidPtr(localPawn))
                return false;

            const uintptr_t camServices = memory::Read<uintptr_t>(localPawn + schema::C_BasePlayerPawn::m_pCameraServices);
            if (!IsValidPtr(camServices))
                return false;

            const uint32_t ppHandle = memory::Read<uint32_t>(
                camServices + schema::CPlayer_CameraServices::m_hActivePostProcessingVolume);

            if (!ppHandle || ppHandle == 0xFFFFFFFF)
                return false;

            const uintptr_t postProcess = memory::ResolveHandle(entityList, ppHandle);
            if (!IsValidPtr(postProcess) || !HasSceneNode(postProcess))
                return false;

            memory::Write<float>(postProcess + schema::C_PostProcessingVolume::m_flMinExposure, exposure);
            memory::Write<float>(postProcess + schema::C_PostProcessingVolume::m_flMaxExposure, exposure);
            return true;
        }

        void RestorePostProcess(uintptr_t entityList, uintptr_t localPawn) {
            ApplyPostProcess(entityList, localPawn, kDefaultExposure);
        }

        void ApplySky(float brightness) {
            for (const uintptr_t sky : g_skyEntities) {
                if (!HasSceneNode(sky))
                    continue;

                memory::Write<float>(sky + schema::C_EnvSky::m_flBrightnessScale, brightness);
                memory::Write<bool>(sky + schema::C_EnvSky::m_bEnabled, true);
            }
        }

        void RestoreSky() {
            constexpr ColorRGBA kWhite{ 255, 255, 255, 255 };

            for (const uintptr_t sky : g_skyEntities) {
                if (!HasSceneNode(sky))
                    continue;

                memory::Write<ColorRGBA>(sky + schema::C_EnvSky::m_vTintColor, kWhite);
                memory::Write<float>(sky + schema::C_EnvSky::m_flBrightnessScale, kDefaultSkyBrightness);
            }
        }
    }

    void Run() {
        CConfig* cfg = CConfig::get();
        const bool enabled = cfg->b["visuals_night_mode"];

        const uintptr_t client = memory::GetModuleBase("client.dll");
        if (!client || !game_state::IsInMatch()) {
            ResetState();
            return;
        }

        uintptr_t entityList = 0;
        uintptr_t localController = 0;
        uintptr_t localPawn = 0;
        if (!game_state::GetClientPointers(client, entityList, localController, localPawn)
            || !IsValidPtr(localPawn)) {
            ResetState();
            return;
        }

        if (!enabled) {
            if (g_wasEnabled) {
                RestorePostProcess(entityList, localPawn);
                RestoreSky();
            }
            ResetState();
            return;
        }

        if (!g_skyScanned)
            DiscoverSkyEntities(entityList);
        else
            PruneSkyEntities();

        const float exposure = cfg->f["visuals_night_mode_exposure"];
        const float skyBrightness = cfg->f["visuals_night_mode_sky_brightness"];

        if (ApplyPostProcess(entityList, localPawn, exposure))
            ApplySky(skyBrightness);

        g_wasEnabled = true;
    }
}
