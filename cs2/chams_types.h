#pragma once

#include <cstdint>
#include "imgui/imgui.h"

namespace chams {
    class c_material2 {
    public:
        virtual const char* get_name() = 0;
        virtual const char* get_share_name() = 0;
    };

    struct resource_binding_t {
        void* data = nullptr;
    };

    template<typename T>
    class c_strong_handle {
    public:
        operator T*() const {
            if (!binding)
                return nullptr;
            return static_cast<T*>(binding->data);
        }

        T* operator->() const {
            return operator T*();
        }

        const resource_binding_t* binding = nullptr;
    };

    using material_handle_t = c_strong_handle<c_material2>;

    struct kv3_id_t {
        const char* name = "generic";
        std::uint64_t unk0 = 0x469806E97412167CULL;
        std::uint64_t unk1 = 0xE73790B53EE6F2AFULL;
    };

    struct c_key_values3 {
        char pad[0x100]{};
    };

    struct c_mesh_draw_primitive {
        char pad0[0x20]{};
        c_material2* material = nullptr;
        c_material2* material2 = nullptr;
        char pad1[0x10]{};
        ImColor tint_color{};

        template<typename T>
        T* get_scene_object() const {
            return reinterpret_cast<T*>(reinterpret_cast<std::uintptr_t>(this) + 0x18);
        }
    };

    struct c_mesh_primitive_output_buffer {
        c_mesh_draw_primitive* out = nullptr;
        int max_output_primitives = 0;
        int start_primitive = 0;
    };

    struct c_scene_animatable_object {
        char pad0[0xC0]{};
        std::uint32_t owner_handle = 0;
    };

    using generate_primitives_fn = void(__fastcall*)(void*, void*, void*, c_mesh_primitive_output_buffer*);
}
