#pragma once

#include <cstdint>

struct world_vec3 {
    float x, y, z;
};

enum class e_light_object_type : std::uint32_t {
    none = 0,
    omni_light,
    directional_light,
    spot_light,
    ortho_light,
};

class c_light_data {
public:
    template<typename T>
    T& get(std::uint32_t offset) {
        return *reinterpret_cast<T*>(reinterpret_cast<std::uintptr_t>(this) + offset);
    }

    e_light_object_type& type() { return get<e_light_object_type>(0x0); }
    world_vec3& color() { return get<world_vec3>(0x4); }
    float& range() { return get<float>(0x1c); }
    bool& shadows() { return get<bool>(0x3D); }
    std::uint8_t& shadow_slot() { return get<std::uint8_t>(0x74); }
    bool& enabled() { return get<bool>(0x75); }
    float& rot_x() { return get<float>(0xa4); }
    float& rot_y() { return get<float>(0xa8); }
    float& alpha() { return get<float>(0xac); }
};

class C_SceneLightObject {
public:
    c_light_data* data() {
        return reinterpret_cast<c_light_data*>(reinterpret_cast<std::uintptr_t>(this) + 0xE0);
    }
};
