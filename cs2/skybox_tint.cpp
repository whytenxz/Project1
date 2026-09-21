#include "skybox_tint.h"
#include "../Backend/Config/Config.h"

#include <Windows.h>
#include <cstring>

namespace skybox_tint {
    namespace {
        constexpr std::ptrdiff_t kSkyboxTintOffset = 0xE8;

        void WriteSkyboxTint(void* scene_skybox_obj, const float col[4]) {
            __try {
                std::memcpy(reinterpret_cast<void*>(reinterpret_cast<std::uintptr_t>(scene_skybox_obj) + kSkyboxTintOffset), col, sizeof(float) * 4);
            } __except (EXCEPTION_EXECUTE_HANDLER) {
            }
        }
    }

    void OnDrawSkyboxArray(void* scene_skybox_obj) {
        CConfig* cfg = CConfig::get();
        if (!cfg->b["visuals_skybox_tint"] || !scene_skybox_obj)
            return;

        const float col[4] = {
            cfg->c["visuals_skybox_tint_color"][0] / 255.f,
            cfg->c["visuals_skybox_tint_color"][1] / 255.f,
            cfg->c["visuals_skybox_tint_color"][2] / 255.f,
            cfg->c["visuals_skybox_tint_color"][3] / 255.f
        };

        WriteSkyboxTint(scene_skybox_obj, col);
    }
}
