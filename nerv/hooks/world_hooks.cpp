#include "world_hooks.hpp"
#include "../../cs2/skybox_tint.h"
#include "../../cs2/world_modulation.h"
#include "../../cs2/offsets.h"
#include "../../cs2/world_modulation_types.h"
#include "../../kiero/minhook/include/MinHook.h"
#include "../main.hpp"

class c_hook {
    void* m_function = nullptr;
    void* m_detour = nullptr;
    void* m_trampoline = nullptr;
public:
    bool hook(void* target, void* detour) {
        m_function = target;
        m_detour = detour;

        if (MH_CreateHook(m_function, m_detour, &m_trampoline) != MH_OK)
            return false;

        return MH_EnableHook(m_function) == MH_OK;
    }

    template<typename TOriginal>
    TOriginal get_original() const {
        return reinterpret_cast<TOriginal>(m_trampoline);
    }
};

namespace hooks {
    namespace light_scene_object {
        inline c_hook hook{};

        void* __fastcall hk_light_scene_object(void* a1, C_SceneLightObject* object, void* a3) {
            world_modulation::OnLightSceneObject(object);

            const auto original = hook.get_original<void*(__fastcall*)(void*, C_SceneLightObject*, void*)>();
            return original ? original(a1, object, a3) : nullptr;
        }
    }

    namespace draw_skybox_array {
        inline c_hook hook{};

        void __fastcall hk_draw_skybox_array(void* a1, void* a2, void* a3, bool draw_skybox, void* a5, void* a6, void* a7) {
            if (a3) {
                void* skybox_obj = *reinterpret_cast<void**>(reinterpret_cast<std::uintptr_t>(a3) + 0x18);
                if (skybox_obj)
                    skybox_tint::OnDrawSkyboxArray(skybox_obj);
            }

            const auto original = hook.get_original<void(__fastcall*)(void*, void*, void*, bool, void*, void*, void*)>();
            if (original)
                original(a1, a2, a3, draw_skybox, a5, a6, a7);
        }
    }
}

bool c_world_hooks::initialize() {
    constexpr const char* kSceneSystem = "scenesystem.dll";

    const auto light_scene = g_opcodes->scan(kSceneSystem, offsets::signatures::light_scene_object);
    if (light_scene)
        hooks::light_scene_object::hook.hook(light_scene, hooks::light_scene_object::hk_light_scene_object);

    const auto draw_skybox = g_opcodes->scan(kSceneSystem, offsets::signatures::draw_skybox_array);
    if (draw_skybox)
        hooks::draw_skybox_array::hook.hook(draw_skybox, hooks::draw_skybox_array::hk_draw_skybox_array);

    return light_scene != nullptr || draw_skybox != nullptr;
}
