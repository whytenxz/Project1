#include "world_modulation.h"
#include "../Backend/Config/Config.h"

#include <Windows.h>

namespace world_modulation {
    namespace {
        void ApplyLightData(c_light_data* data, bool disable, float r, float g, float b, bool shadows, bool change_rotation, float rot_x, float rot_y, bool baked_shadows) {
            __try {
                if (disable) {
                    data->shadow_slot() = 0xFF;
                    data->enabled() = false;
                    data->color() = world_vec3{};
                    data->alpha() = 0.f;
                    return;
                }

                data->color() = world_vec3{ r * 3.f, g * 3.f, b * 3.f };
                data->shadows() = shadows;

                if (change_rotation) {
                    data->rot_x() = rot_x;
                    data->rot_y() = rot_y;
                }

                data->alpha() = 1.f;
                data->shadow_slot() = baked_shadows ? 0 : 1;
                data->enabled() = true;
            } __except (EXCEPTION_EXECUTE_HANDLER) {
            }
        }
    }

    void OnLightSceneObject(C_SceneLightObject* object) {
        CConfig* cfg = CConfig::get();
        if (!cfg->b["visuals_light_modulation"] || !object)
            return;

        c_light_data* data = object->data();
        if (!data || data->type() != e_light_object_type::directional_light)
            return;

        ApplyLightData(
            data,
            cfg->b["visuals_light_disable"],
            cfg->c["visuals_light_color"][0] / 255.f,
            cfg->c["visuals_light_color"][1] / 255.f,
            cfg->c["visuals_light_color"][2] / 255.f,
            cfg->b["visuals_light_shadows"],
            cfg->b["visuals_light_change_rotation"],
            cfg->f["visuals_light_rot_x"],
            cfg->f["visuals_light_rot_y"],
            cfg->b["visuals_light_baked_shadows"]);
    }
}
