#include "chams.h"
#include "chams_types.h"
#include "game_state.h"
#include "memory.h"
#include "offsets.h"
#include "../Backend/Config/Config.h"
#include "../kiero/minhook/include/MinHook.h"

#include <Windows.h>
#include <iostream>
#include <vector>
#include <ranges>
#include <iomanip>
#include <cstddef>
#include <algorithm>
#include <string>

namespace chams {
    namespace {
        std::vector<int> ida_to_bytes(const char* pattern)
        {
            std::vector<int> bytes = std::vector<int>{};
            char* start = const_cast<char*>(pattern);
            char* end = const_cast<char*>(pattern) + strlen(pattern);

            for (char* current = start; current < end; ++current) {
                if (*current == '?') {
                    ++current;

                    if (*current == '?')
                        ++current;

                    bytes.push_back(-1);
                }
                else {
                    bytes.push_back(strtoul(current, &current, 16));
                }
            }

            return bytes;
        }

        uint8_t* scan(const char* module_name, const char* pattern) {
            void* module_handle = GetModuleHandleA(module_name);
            if (module_handle == nullptr)
                return nullptr;

            PIMAGE_DOS_HEADER dos_header = reinterpret_cast<PIMAGE_DOS_HEADER>(module_handle);
            PIMAGE_NT_HEADERS nt_headers = reinterpret_cast<PIMAGE_NT_HEADERS>(reinterpret_cast<uint8_t*>(module_handle) + dos_header->e_lfanew);

            auto size_of_image = nt_headers->OptionalHeader.SizeOfImage;
            auto pattern_bytes = ida_to_bytes(pattern);
            auto scan_bytes = reinterpret_cast<uint8_t*>(module_handle);

            auto pattern_size = pattern_bytes.size();
            auto pattern_data = pattern_bytes.data();

            for (unsigned int i = 0; i < size_of_image - pattern_size; i++) {
                bool found = true;

                for (unsigned int j = 0; j < pattern_size; ++j) {
                    if (pattern_data[j] == -1)
                        continue;

                    if (scan_bytes[i + j] != pattern_data[j]) {
                        found = false;
                        break;
                    }
                }

                if (found)
                    return &scan_bytes[i];
            }
            return nullptr;
        }

        struct unk_data_t {
            uint8_t _pad0[0x98];
            const char* m_panel_name;
        };

        struct CSceneAnimatableObject {
            uint8_t _pad0[0xB8];
            unk_data_t* m_unk_data;
            std::uint32_t m_owner;
            uint8_t _pad1[0x4C];

            std::uint32_t owner() {
                return m_owner;
            }
        };

        class CSceneObject
        {
        public:
            char pad0[0x78];
            std::uint32_t flags;
        };

        struct kv3_id {
            const char* m_name;
            std::uint64_t m_hash1;
            std::uint64_t m_hash2;
        };

        class utl_buffer {
        public:
            char pad0[0x80];

            utl_buffer(int a1, int size, int a3) {
                using fn = void(__stdcall*)(utl_buffer*, int, int, int);
                static auto func = (fn)scan("tier0.dll", "40 55 56 41 57 48 83 EC ? 33 ED");
                func(this, a1, size, a3);
            }

            utl_buffer(void* a1, int size, int a3) {
                using fn = void(__stdcall*)(utl_buffer*, void*, int, int);
                static auto func = (fn)scan("tier0.dll", "48 89 5C 24 ? 57 48 83 EC ? 33 FF 41 8B C0");
                func(this, a1, size, a3);
            }

            void put_str(const char* str) {
                using fn = void(__stdcall*)(utl_buffer*, const char*);
                static auto func = (fn)scan("tier0.dll", "40 53 57 48 83 EC ? 0F B6 41");
                func(this, str);
            }

            void ensure_capacity(int size) {
                using fn = void(__stdcall*)(utl_buffer*, int);
                static auto func = (fn)scan("tier0.dll", "48 89 74 24 ? 57 48 83 EC ? 8B 41 ? 8D 72");
                func(this, size);
            }
        };

        class key_values_3 {
        public:
            char pad0[0x100];
            std::uint64_t m_key;
            void* m_value;
            char pad1[0x8];

            void load_from_buffer(const char* str) {
                utl_buffer buffer(0, static_cast<int32_t> (strlen(str) + 10), 0);
                buffer.put_str(str);
                load_kv3(&buffer);
            }

            static key_values_3* create_material_resource() {
                using fn = key_values_3 * (__fastcall*)(key_values_3*, std::uint32_t, std::uint32_t);
                static auto func = (fn)scan("client.dll", "40 53 48 83 EC ? 4C 8B 11 41 B9");

                key_values_3* new_kv = new key_values_3[2 * sizeof(void*)];
                return func(new_kv, 1U, 6U);
            }

            bool load_kv3(utl_buffer* buffer) {
                using fn_t = bool(__fastcall*)(key_values_3*, void*, utl_buffer*, kv3_id*, void*, void*, void*, void*, const char*);
                static auto fn = (fn_t)scan("tier0.dll", "48 89 5C 24 ? 48 89 6C 24 ? 48 89 7C 24 ? 41 54 41 56 41 57 48 83 EC ? 45 33 E4");

                kv3_id id = kv3_id{ "generic", 0x41B818518343427EULL, 0xB5F447C23C0CDF8CULL };
                return fn(this, nullptr, buffer, &id, nullptr, nullptr, nullptr, nullptr, "");
            }
        };

        struct MaterialParam {
            float vec[4];
        };

        class material_2 {
        public:
            virtual const char* get_name() = 0;
            virtual const char* get_name_with_mod() = 0;
            virtual void func2() = 0;
            virtual void func3() = 0;
            virtual bool is_loaded() = 0;

            MaterialParam* find_parameter(const char* name) {
                using fn_find_parameter = MaterialParam * (__fastcall*)(material_2*, const char*);
                static const auto find_parameter = (fn_find_parameter)scan("materialsystem2.dll", "48 89 5C 24 ? 48 89 74 24 ?? 57 48 83 EC 20 48 8B 59 18");
                return find_parameter(this, name);
            }

            void update_parameter() {
                using fn = void* (__fastcall*)(material_2*);
                static auto update_param_fn = (fn)scan("materialsystem2.dll", "48 89 7C 24 ? 41 56 48 83 EC ? 8B 81");
                update_param_fn(this);
            }
        };

        template <std::size_t Index, typename ReturnType, typename... Args>
        __forceinline ReturnType call_virtual(void* instance, Args... args)
        {
            using Fn = ReturnType(__thiscall*)(void*, Args...);

            auto function = (*reinterpret_cast<Fn**>(instance))[Index];
            return function(instance, args...);
        }

        class material_system_2 {
        public:
            material_2*** find_or_create_from_resource(material_2*** out_mat, const char* material_name) {
                return call_virtual<14, material_2***>(this, out_mat, material_name);
            }

            material_2** create_material(material_2*** out_mat, const char* material_name, void* data) {
                return call_virtual<29, material_2**>(this, out_mat, material_name, data, 0, 0, 0, 0, 0, 1);
            }

            void set_create_data_by_material(const void* data, material_2*** const in_mat) {
                return call_virtual<37, void>(this, in_mat, data);
            }
        };

        class CMeshData
        {
        public:
            void* m_pData;
            char pad0[0x10];
            CSceneAnimatableObject* m_pSceneAnimatableObject;
            material_2* m_pMaterial;
            material_2* m_pMaterial2;
            char pad1[0x10];
            void* some_data;
            char pad2[0x8];
            uint8_t m_pColor[4];
            char pad3[0x14];
        };

        class CMeshPrimitiveOutputBuffer {
        public:
            CMeshData* m_Out;
            int m_nMaxOutputPrimitives;
            int m_nStartPrimitives;
        };

        void (__fastcall* oGeneratePrimitives)(void*, CSceneAnimatableObject*, void*, CMeshPrimitiveOutputBuffer*) = nullptr;
        bool g_initialized = false;
        bool g_hooked = false;

        material_2** enemy_mat_invis = nullptr;
        material_2** enemy_mat_vis = nullptr;
        material_2** team_mat_invis = nullptr;
        material_2** team_mat_vis = nullptr;

        float g_last_visible_color[4] = {0.0f, 1.0f, 0.0f, 1.0f};
        float g_last_hidden_color[4] = {1.0f, 0.0f, 0.0f, 1.0f};
        int g_last_cham_style = -1;

        float g_last_team_visible_color[4] = {0.0f, 1.0f, 0.0f, 1.0f};
        float g_last_team_hidden_color[4] = {1.0f, 0.0f, 0.0f, 1.0f};
        int g_last_team_cham_style = -1;

        auto create_material = [](const char* material_name, const char vmat[]) -> material_2** {
            if (!material_name || !vmat)
                return nullptr;

            key_values_3* keyval = key_values_3::create_material_resource();
            if (!keyval)
                return nullptr;

            keyval->load_from_buffer(vmat);

            material_2** out_mat = nullptr;
            material_system_2* mat_system = nullptr;
            auto hMat = GetModuleHandleA("materialsystem2.dll");
            if (hMat)
            {
                using CreateInterface_t = void* (__cdecl*)(const char*, int*);
                auto fnCreateInterface = (CreateInterface_t)GetProcAddress(hMat, "CreateInterface");
                mat_system = (material_system_2*)fnCreateInterface("VMaterialSystem2_001", nullptr);
            }
            if (!mat_system)
                return nullptr;

            mat_system->create_material(reinterpret_cast<material_2***>(&out_mat), material_name, keyval);

            return out_mat;
        };

        uintptr_t get_entity_from_handle(std::uint32_t handle)
        {
            if (handle == 0xFFFFFFFF)
                return 0;
            const uintptr_t client = memory::GetModuleBase("client.dll");
            if (!client)
                return 0;
            const uintptr_t entityList = memory::Read<uintptr_t>(client + offsets::client_dll::dwEntityList);
            if (!entityList)
                return 0;
            return memory::ResolveHandle(entityList, handle);
        }

        std::string generate_vmat_flat_visible(float color[4])
        {
            char buffer[1024];
            sprintf_s(buffer, 1024, R"(<!-- kv3 encoding:text:version{e21c7f3c-8a33-41c5-9977-a76d3a32aa0d}
			format:generic:version{7412167c-06e9-4698-aff2-e63eb59037e7} -->
			{
                shader = "solidcolor.vfx"
                g_tColor = resource:"materials/dev/primary_white_color_tga_21186c76.vtex"
                g_tNormal = resource:"materials/default/default_normal_tga_7652cb.vtex"
                g_tRoughness = resource:"materials/default/default_normal_tga_b3f4ec4c.vtex"
                g_tMetalness = resource:"materials/default/default_normal_tga_b3f4ec4c.vtex"
                g_tAmbientOcclusion = resource:"materials/default/default_normal_tga_b3f4ec4c.vtex"
                F_IGNOREZ = 0
                F_DISABLE_Z_WRITE = 0
                F_DISABLE_Z_BUFFERING = 0
                F_RENDER_BACKFACES = 1
                g_vColorTint = [%.2f, %.2f, %.2f, %.2f]
			})", color[0], color[1], color[2], color[3]);
            return std::string(buffer);
        }

        std::string generate_vmat_flat_hidden(float color[4])
        {
            char buffer[1024];
            sprintf_s(buffer, 1024, R"(<!-- kv3 encoding:text:version{e21c7f3c-8a33-41c5-9977-a76d3a32aa0d}
			format:generic:version{7412167c-06e9-4698-aff2-e63eb59037e7} -->
			{
				shader = "solidcolor.vfx"
				g_tColor = resource:"materials/dev/primary_white_color_tga_21186c76.vtex"
				g_tNormal = resource:"materials/default/default_normal_tga_7652cb.vtex"
				g_tRoughness = resource:"materials/default/default_normal_tga_b3f4ec4c.vtex"
				g_tMetalness = resource:"materials/default/default_normal_tga_b3f4ec4c.vtex"
				g_tAmbientOcclusion = resource:"materials/default/default_normal_tga_b3f4ec4c.vtex"
				F_IGNOREZ = 1
				F_DISABLE_Z_WRITE = 1
				F_DISABLE_Z_BUFFERING = 1
				F_RENDER_BACKFACES = 1
				g_vColorTint = [%.2f, %.2f, %.2f, %.2f]
			})", color[0], color[1], color[2], color[3]);
            return std::string(buffer);
        }

        std::string generate_vmat_solid_visible(float color[4])
        {
            char buffer[1024];
            sprintf_s(buffer, 1024, R"(<!-- kv3 encoding:text:version{e21c7f3c-8a33-41c5-9977-a76d3a32aa0d}
			format:generic:version{7412167c-06e9-4698-aff2-e63eb59037e7} -->
			{
                shader = "generic.vfx"
                g_tColor = resource:"materials/dev/primary_white_color_tga_21186c76.vtex"
                g_tNormal = resource:"materials/default/default_normal_tga_7652cb.vtex"
                g_tRoughness = resource:"materials/default/default_normal_tga_b3f4ec4c.vtex"
                g_tMetalness = resource:"materials/default/default_normal_tga_b3f4ec4c.vtex"
                g_tAmbientOcclusion = resource:"materials/default/default_normal_tga_b3f4ec4c.vtex"
                F_IGNOREZ = 0
                F_DISABLE_Z_WRITE = 0
                F_DISABLE_Z_BUFFERING = 0
                F_RENDER_BACKFACES = 1
                g_vColorTint = [%.2f, %.2f, %.2f, %.2f]
			})", color[0], color[1], color[2], color[3]);
            return std::string(buffer);
        }

        std::string generate_vmat_solid_hidden(float color[4])
        {
            char buffer[1024];
            sprintf_s(buffer, 1024, R"(<!-- kv3 encoding:text:version{e21c7f3c-8a33-41c5-9977-a76d3a32aa0d}
			format:generic:version{7412167c-06e9-4698-aff2-e63eb59037e7} -->
			{
                shader = "solidcolor.vfx"
                g_tColor = resource:"materials/dev/primary_white_color_tga_21186c76.vtex"
                g_tNormal = resource:"materials/default/default_normal_tga_7652cb.vtex"
                g_tRoughness = resource:"materials/default/default_normal_tga_b3f4ec4c.vtex"
                g_tMetalness = resource:"materials/default/default_normal_tga_b3f4ec4c.vtex"
                g_tAmbientOcclusion = resource:"materials/default/default_normal_tga_b3f4ec4c.vtex"
                F_IGNOREZ = 1
                F_DISABLE_Z_WRITE = 1
                F_DISABLE_Z_BUFFERING = 1
                F_RENDER_BACKFACES = 1
                g_vColorTint = [%.2f, %.2f, %.2f, %.2f]
			})", color[0], color[1], color[2], color[3]);
            return std::string(buffer);
        }

        std::string generate_vmat_latex_visible(float color[4])
        {
            char buffer[1024];
            sprintf_s(buffer, 1024, R"(<!-- kv3 encoding:text:version{e21c7f3c-8a33-41c5-9977-a76d3a32aa0d}
			format:generic:version{7412167c-06e9-4698-aff2-e63eb59037e7} -->
			{
                shader = "csgo_character.vfx"
                F_BLEND_MODE = 1
                g_vColorTint = [%.2f, %.2f, %.2f, %.2f]
                g_bFogEnabled = 0
                g_flMetalness = 0.000
                g_tMetalness = resource:"materials/default/default_metal_tga_8fbc2820.vtex"
                g_tColor = resource:"materials/dev/primary_white_color_tga_21186c76.vtex"
                g_tAmbientOcclusion = resource:"materials/default/default_ao_tga_79a2e0d0.vtex"
                g_tNormal = resource:"materials/default/default_normal_tga_1b833b2a.vtex"
			})", color[0], color[1], color[2], color[3]);
            return std::string(buffer);
        }

        std::string generate_vmat_latex_hidden(float color[4])
        {
            char buffer[1024];
            sprintf_s(buffer, 1024, R"(<!-- kv3 encoding:text:version{e21c7f3c-8a33-41c5-9977-a76d3a32aa0d}
			format:generic:version{7412167c-06e9-4698-aff2-e63eb59037e7} -->
			{
                shader = "csgo_character.vfx"
                F_DISABLE_Z_BUFFERING = 1
                F_DISABLE_Z_PREPASS = 1
                F_DISABLE_Z_WRITE = 1
                F_BLEND_MODE = 1
                g_vColorTint = [%.2f, %.2f, %.2f, %.2f]
                g_bFogEnabled = 0
                g_flMetalness = 0.000
                g_tColor = resource:"materials/dev/primary_white_color_tga_21186c76.vtex"
                g_tAmbientOcclusion = resource:"materials/default/default_ao_tga_79a2e0d0.vtex"
                g_tNormal = resource:"materials/default/default_normal_tga_1b833b2a.vtex"
                g_tMetalness = resource:"materials/default/default_metal_tga_8fbc2820.vtex"
			})", color[0], color[1], color[2], color[3]);
            return std::string(buffer);
        }

        std::string generate_vmat_chrome_visible(float color[4])
        {
            char buffer[1024];
            sprintf_s(buffer, 1024, R"(<!-- kv3 encoding:text:version{e21c7f3c-8a33-41c5-9977-a76d3a32aa0d} format:generic:version{7412167c-06e9-4698-aff2-e63eb59037e7} -->
			{
                shader = "csgo_complex.vfx"
                F_PAINT_VERTEX_COLORS = 1
                F_TRANSLUCENT = 1
                F_SPECULAR = 1
                F_RENDER_BACKFACES = 0
                g_flAmbientOcclusionDirectSpecular = 1.000000
                g_flMetalness = 0.000000
                g_flRoughness = 0.050000
                g_vColorTint = [%.2f, %.2f, %.2f, %.2f]
                g_tColor = resource:"materials/dev/primary_white_color_tga_21186c76.vtex"
                g_tNormal = resource:"materials/default/default_normal_tga_7652cb.vtex"
                g_tAmbientOcclusion = resource:"materials/default/default_ao_tga_79a2e0d0.vtex"
			})", color[0], color[1], color[2], color[3]);
            return std::string(buffer);
        }

        std::string generate_vmat_chrome_hidden(float color[4])
        {
            char buffer[1024];
            sprintf_s(buffer, 1024, R"(<!-- kv3 encoding:text:version{e21c7f3c-8a33-41c5-9977-a76d3a32aa0d} format:generic:version{7412167c-06e9-4698-aff2-e63eb59037e7} -->
			{
                shader = "csgo_complex.vfx"
                F_PAINT_VERTEX_COLORS = 1
                F_TRANSLUCENT = 1
                F_SPECULAR = 1
                F_RENDER_BACKFACES = 0
                F_DISABLE_Z_BUFFERING = 1
                F_DISABLE_Z_PREPASS = 0
                F_DISABLE_Z_WRITE = 1
                g_flAmbientOcclusionDirectSpecular = 1.000000
                g_flMetalness = 0.000000
                g_flRoughness = 0.050000
                g_vColorTint = [%.2f, %.2f, %.2f, %.2f]
                g_tColor = resource:"materials/dev/primary_white_color_tga_21186c76.vtex"
                g_tNormal = resource:"materials/default/default_normal_tga_7652cb.vtex"
                g_tAmbientOcclusion = resource:"materials/default/default_ao_tga_79a2e0d0.vtex"
			})", color[0], color[1], color[2], color[3]);
            return std::string(buffer);
        }

        std::string generate_vmat_flow_visible(float color[4])
        {
            char buffer[1024];
            sprintf_s(buffer, 1024, R"(<!-- kv3 encoding:text:version{e21c7f3c-8a33-41c5-9977-a76d3a32aa0d} format:generic:version{7412167c-06e9-4698-aff2-e63eb59037e7} -->
{
    shader = "csgo_effects.vfx"
    g_tColor = resource:"materials/dev/primary_white_color_tga_21186c76.vtex"
    g_tMask1 = resource:"materials/default/default_mask_tga_344101f8.vtex"
    g_tMask2 = resource:"materials/default/default_mask_tga_344101f8.vtex"
    g_tMask3 = resource:"materials/default/default_mask_tga_344101f8.vtex"
    g_flColorBoost = 20.0
    g_flOpacityScale = %.6f
    g_flFresnelExponent = 10.0
    g_flFresnelFalloff = 10.0
    g_flFresnelMax = 0.0
    g_flFresnelMin = 1.0
    F_ADDITIVE_BLEND = 1
    F_BLEND_MODE = 1
    F_TRANSLUCENT = 1
    F_IGNOREZ = 0
    F_DISABLE_Z_BUFFERING = 0
    F_RENDER_BACKFACES = 0
    g_vColorTint = [%.5f, %.5f, %.5f]
})", color[3], color[0], color[1], color[2]);
            return std::string(buffer);
        }

        std::string generate_vmat_flow_hidden(float color[4])
        {
            char buffer[1024];
            sprintf_s(buffer, 1024, R"(<!-- kv3 encoding:text:version{e21c7f3c-8a33-41c5-9977-a76d3a32aa0d} format:generic:version{7412167c-06e9-4698-aff2-e63eb59037e7} -->
{
    shader = "csgo_effects.vfx"
    g_tColor = resource:"materials/dev/primary_white_color_tga_21186c76.vtex"
    g_tMask1 = resource:"materials/default/default_mask_tga_344101f8.vtex"
    g_tMask2 = resource:"materials/default/default_mask_tga_344101f8.vtex"
    g_tMask3 = resource:"materials/default/default_mask_tga_344101f8.vtex"
    g_flColorBoost = 20.0
    g_flOpacityScale = %.6f
    g_flFresnelExponent = 10.0
    g_flFresnelFalloff = 10.0
    g_flFresnelMax = 0.0
    g_flFresnelMin = 1.0
    F_ADDITIVE_BLEND = 1
    F_BLEND_MODE = 1
    F_TRANSLUCENT = 1
    F_IGNOREZ = 1
    F_DISABLE_Z_BUFFERING = 1
    F_RENDER_BACKFACES = 0
    g_vColorTint = [%.5f, %.5f, %.5f]
})", color[3], color[0], color[1], color[2]);
            return std::string(buffer);
        }

        void hkGeneratePrimitives(void* a1, CSceneAnimatableObject* object, void* a3, CMeshPrimitiveOutputBuffer* render_buf)
        {
            if (!oGeneratePrimitives) {
                return;
            }

            CConfig* cfg = CConfig::get();
            const bool chams_enemies_enabled = cfg->b["visuals_chams"];
            const bool chams_teammates_enabled = cfg->b["visuals_chams_teammates"];

            if (!chams_enemies_enabled && !chams_teammates_enabled) {
                oGeneratePrimitives(a1, object, a3, render_buf);
                return;
            }

            // Skip if this is a weapon model to prevent interference with knife changer
            std::uint32_t hOwnerHandle = object->m_owner;
            if (hOwnerHandle != 0) {
                uintptr_t entity = get_entity_from_handle(hOwnerHandle);
                if (entity) {
                    std::uintptr_t entity_info = memory::Read<std::uintptr_t>(entity + 0x10);
                    if (entity_info) {
                        std::uintptr_t entity_type = memory::Read<std::uintptr_t>(entity_info + 0x20);
                        if (entity_type) {
                            char type[128]{};
                            memory::detail::ReadBytes(entity_type, type, sizeof(type) - 1);
                            if (std::string(type) == "cs2_hudmodel_weapon") {
                                oGeneratePrimitives(a1, object, a3, render_buf);
                                return;
                            }
                        }
                    }
                }
            }

            const bool draw_hidden_enemy = cfg->b["visuals_chams_ignorez"];
            const bool draw_hidden_team = cfg->b["visuals_chams_teammates_ignorez"];

            auto draw_chams_for_mesh = [&](CMeshData* mesh, bool is_enemy) {
                if (is_enemy) {
                    if (draw_hidden_enemy) {
                        mesh->m_pMaterial = enemy_mat_invis ? *enemy_mat_invis : mesh->m_pMaterial;
                        mesh->m_pMaterial2 = enemy_mat_invis ? *enemy_mat_invis : mesh->m_pMaterial2;
                    } else {
                        mesh->m_pMaterial = enemy_mat_vis ? *enemy_mat_vis : mesh->m_pMaterial;
                        mesh->m_pMaterial2 = enemy_mat_vis ? *enemy_mat_vis : mesh->m_pMaterial2;
                    }
                } else {
                    if (draw_hidden_team) {
                        mesh->m_pMaterial = team_mat_invis ? *team_mat_invis : mesh->m_pMaterial;
                        mesh->m_pMaterial2 = team_mat_invis ? *team_mat_invis : mesh->m_pMaterial2;
                    } else {
                        mesh->m_pMaterial = team_mat_vis ? *team_mat_vis : mesh->m_pMaterial;
                        mesh->m_pMaterial2 = team_mat_vis ? *team_mat_vis : mesh->m_pMaterial2;
                    }
                }
            };

            int prev_count = render_buf->m_nStartPrimitives;
            oGeneratePrimitives(a1, object, a3, render_buf);

            if (prev_count <= render_buf->m_nStartPrimitives)
            {
                for (int i = prev_count; i < render_buf->m_nStartPrimitives; ++i)
                {
                    auto mesh = &render_buf->m_Out[i];

                    std::uint32_t hOwner = object->m_owner;
                    if (hOwner == 0)
                        continue;
                    uintptr_t entity = get_entity_from_handle(hOwner);
                    if (!entity)
                        continue;
                    std::uintptr_t entity_info = memory::Read<std::uintptr_t>(entity + 0x10);
                    if (!entity_info)
                        continue;
                    std::uintptr_t entity_type = memory::Read<std::uintptr_t>(entity_info + 0x20);
                    if (!entity_type)
                        continue;

                    char type[128]{};
                    memory::detail::ReadBytes(entity_type, type, sizeof(type) - 1);
                    if (std::string(type) == "c_cs_player_for_precache")
                    {
                        int health = memory::Read<int>(entity + schema::C_BaseEntity::m_iHealth);
                        if (health <= 0 || health > 100)
                            continue;

                        int enemy_teamnum = memory::Read<int>(entity + schema::C_BaseEntity::m_iTeamNum);
                        
                        const uintptr_t client = memory::GetModuleBase("client.dll");
                        if (!client)
                            continue;
                        const uintptr_t local_pawn = memory::Read<uintptr_t>(client + offsets::client_dll::dwLocalPlayerPawn);
                        if (!local_pawn)
                            continue;
                        int locplayer_teamnum = memory::Read<int>(local_pawn + schema::C_BaseEntity::m_iTeamNum);

                        bool is_enemy = enemy_teamnum != locplayer_teamnum;
                        
                        if (is_enemy && !chams_enemies_enabled)
                            continue;
                        if (!is_enemy && !chams_teammates_enabled)
                            continue;

                        CSceneObject* scene_object = (CSceneObject*)(object);
                        scene_object->flags &= ~(1 << 3); // disable pvs

                        draw_chams_for_mesh(mesh, is_enemy);
                    }
                }
            }
        }

        void initialize_materials()
        {
            CConfig* cfg = CConfig::get();
            int enemy_style = std::clamp(cfg->i["visuals_chams_type"], 0, 4);
            int team_style = std::clamp(cfg->i["visuals_chams_teammates_type"], 0, 4);

            float enemy_vis_col[4] = {
                cfg->c["visuals_chams_visible_color"][0] / 255.f,
                cfg->c["visuals_chams_visible_color"][1] / 255.f,
                cfg->c["visuals_chams_visible_color"][2] / 255.f,
                cfg->c["visuals_chams_visible_color"][3] / 255.f
            };
            float enemy_inv_col[4] = {
                cfg->c["visuals_chams_hidden_color"][0] / 255.f,
                cfg->c["visuals_chams_hidden_color"][1] / 255.f,
                cfg->c["visuals_chams_hidden_color"][2] / 255.f,
                cfg->c["visuals_chams_hidden_color"][3] / 255.f
            };

            float team_vis_col[4] = {
                cfg->c["visuals_chams_teammates_visible_color"][0] / 255.f,
                cfg->c["visuals_chams_teammates_visible_color"][1] / 255.f,
                cfg->c["visuals_chams_teammates_visible_color"][2] / 255.f,
                cfg->c["visuals_chams_teammates_visible_color"][3] / 255.f
            };
            float team_inv_col[4] = {
                cfg->c["visuals_chams_teammates_hidden_color"][0] / 255.f,
                cfg->c["visuals_chams_teammates_hidden_color"][1] / 255.f,
                cfg->c["visuals_chams_teammates_hidden_color"][2] / 255.f,
                cfg->c["visuals_chams_teammates_hidden_color"][3] / 255.f
            };

            std::string evmat_vis, evmat_invis;
            switch (enemy_style) {
                case 1:
                    evmat_vis = generate_vmat_solid_visible(enemy_vis_col);
                    evmat_invis = generate_vmat_solid_hidden(enemy_inv_col);
                    break;
                case 2:
                    evmat_vis = generate_vmat_latex_visible(enemy_vis_col);
                    evmat_invis = generate_vmat_latex_hidden(enemy_inv_col);
                    break;
                case 3:
                    evmat_vis = generate_vmat_chrome_visible(enemy_vis_col);
                    evmat_invis = generate_vmat_chrome_hidden(enemy_inv_col);
                    break;
                case 4:
                    evmat_vis = generate_vmat_flow_visible(enemy_vis_col);
                    evmat_invis = generate_vmat_flow_hidden(enemy_inv_col);
                    break;
                default:
                    evmat_vis = generate_vmat_flat_visible(enemy_vis_col);
                    evmat_invis = generate_vmat_flat_hidden(enemy_inv_col);
                    break;
            }

            std::string tvmat_vis, tvmat_invis;
            switch (team_style) {
                case 1:
                    tvmat_vis = generate_vmat_solid_visible(team_vis_col);
                    tvmat_invis = generate_vmat_solid_hidden(team_inv_col);
                    break;
                case 2:
                    tvmat_vis = generate_vmat_latex_visible(team_vis_col);
                    tvmat_invis = generate_vmat_latex_hidden(team_inv_col);
                    break;
                case 3:
                    tvmat_vis = generate_vmat_chrome_visible(team_vis_col);
                    tvmat_invis = generate_vmat_chrome_hidden(team_inv_col);
                    break;
                case 4:
                    tvmat_vis = generate_vmat_flow_visible(team_vis_col);
                    tvmat_invis = generate_vmat_flow_hidden(team_inv_col);
                    break;
                default:
                    tvmat_vis = generate_vmat_flat_visible(team_vis_col);
                    tvmat_invis = generate_vmat_flat_hidden(team_inv_col);
                    break;
            }

            enemy_mat_invis = create_material("materials/enemy_chams_invis.vmat", evmat_invis.c_str());
            enemy_mat_vis = create_material("materials/enemy_chams_vis.vmat", evmat_vis.c_str());

            team_mat_invis = create_material("materials/team_chams_invis.vmat", tvmat_invis.c_str());
            team_mat_vis = create_material("materials/team_chams_vis.vmat", tvmat_vis.c_str());
        }

        void update_materials()
        {
            CConfig* cfg = CConfig::get();

            // Check enemy chams updates
            int enemy_style = std::clamp(cfg->i["visuals_chams_type"], 0, 4);
            float current_enemy_vis[4] = {
                cfg->c["visuals_chams_visible_color"][0] / 255.f,
                cfg->c["visuals_chams_visible_color"][1] / 255.f,
                cfg->c["visuals_chams_visible_color"][2] / 255.f,
                cfg->c["visuals_chams_visible_color"][3] / 255.f
            };
            float current_enemy_inv[4] = {
                cfg->c["visuals_chams_hidden_color"][0] / 255.f,
                cfg->c["visuals_chams_hidden_color"][1] / 255.f,
                cfg->c["visuals_chams_hidden_color"][2] / 255.f,
                cfg->c["visuals_chams_hidden_color"][3] / 255.f
            };

            bool enemy_vis_changed = (current_enemy_vis[0] != g_last_visible_color[0] ||
                                    current_enemy_vis[1] != g_last_visible_color[1] ||
                                    current_enemy_vis[2] != g_last_visible_color[2] ||
                                    current_enemy_vis[3] != g_last_visible_color[3]);

            bool enemy_inv_changed = (current_enemy_inv[0] != g_last_hidden_color[0] ||
                                    current_enemy_inv[1] != g_last_hidden_color[1] ||
                                    current_enemy_inv[2] != g_last_hidden_color[2] ||
                                    current_enemy_inv[3] != g_last_hidden_color[3]);

            bool enemy_style_changed = (enemy_style != g_last_cham_style);

            // Check teammate chams updates
            int team_style = std::clamp(cfg->i["visuals_chams_teammates_type"], 0, 4);
            float current_team_vis[4] = {
                cfg->c["visuals_chams_teammates_visible_color"][0] / 255.f,
                cfg->c["visuals_chams_teammates_visible_color"][1] / 255.f,
                cfg->c["visuals_chams_teammates_visible_color"][2] / 255.f,
                cfg->c["visuals_chams_teammates_visible_color"][3] / 255.f
            };
            float current_team_inv[4] = {
                cfg->c["visuals_chams_teammates_hidden_color"][0] / 255.f,
                cfg->c["visuals_chams_teammates_hidden_color"][1] / 255.f,
                cfg->c["visuals_chams_teammates_hidden_color"][2] / 255.f,
                cfg->c["visuals_chams_teammates_hidden_color"][3] / 255.f
            };

            bool team_vis_changed = (current_team_vis[0] != g_last_team_visible_color[0] ||
                                    current_team_vis[1] != g_last_team_visible_color[1] ||
                                    current_team_vis[2] != g_last_team_visible_color[2] ||
                                    current_team_vis[3] != g_last_team_visible_color[3]);

            bool team_inv_changed = (current_team_inv[0] != g_last_team_hidden_color[0] ||
                                    current_team_inv[1] != g_last_team_hidden_color[1] ||
                                    current_team_inv[2] != g_last_team_hidden_color[2] ||
                                    current_team_inv[3] != g_last_team_hidden_color[3]);

            bool team_style_changed = (team_style != g_last_team_cham_style);

            if (!enemy_vis_changed && !enemy_inv_changed && !enemy_style_changed &&
                !team_vis_changed && !team_inv_changed && !team_style_changed &&
                enemy_mat_invis && enemy_mat_vis && team_mat_invis && team_mat_vis)
                return;

            for (int i = 0; i < 4; i++) {
                g_last_visible_color[i] = current_enemy_vis[i];
                g_last_hidden_color[i] = current_enemy_inv[i];
                g_last_team_visible_color[i] = current_team_vis[i];
                g_last_team_hidden_color[i] = current_team_inv[i];
            }
            g_last_cham_style = enemy_style;
            g_last_team_cham_style = team_style;

            std::string evmat_vis, evmat_invis;
            switch (enemy_style) {
                case 1:
                    evmat_vis = generate_vmat_solid_visible(current_enemy_vis);
                    evmat_invis = generate_vmat_solid_hidden(current_enemy_inv);
                    break;
                case 2:
                    evmat_vis = generate_vmat_latex_visible(current_enemy_vis);
                    evmat_invis = generate_vmat_latex_hidden(current_enemy_inv);
                    break;
                case 3:
                    evmat_vis = generate_vmat_chrome_visible(current_enemy_vis);
                    evmat_invis = generate_vmat_chrome_hidden(current_enemy_inv);
                    break;
                case 4:
                    evmat_vis = generate_vmat_flow_visible(current_enemy_vis);
                    evmat_invis = generate_vmat_flow_hidden(current_enemy_inv);
                    break;
                default:
                    evmat_vis = generate_vmat_flat_visible(current_enemy_vis);
                    evmat_invis = generate_vmat_flat_hidden(current_enemy_inv);
                    break;
            }

            std::string tvmat_vis, tvmat_invis;
            switch (team_style) {
                case 1:
                    tvmat_vis = generate_vmat_solid_visible(current_team_vis);
                    tvmat_invis = generate_vmat_solid_hidden(current_team_inv);
                    break;
                case 2:
                    tvmat_vis = generate_vmat_latex_visible(current_team_vis);
                    tvmat_invis = generate_vmat_latex_hidden(current_team_inv);
                    break;
                case 3:
                    tvmat_vis = generate_vmat_chrome_visible(current_team_vis);
                    tvmat_invis = generate_vmat_chrome_hidden(current_team_inv);
                    break;
                case 4:
                    tvmat_vis = generate_vmat_flow_visible(current_team_vis);
                    tvmat_invis = generate_vmat_flow_hidden(current_team_inv);
                    break;
                default:
                    tvmat_vis = generate_vmat_flat_visible(current_team_vis);
                    tvmat_invis = generate_vmat_flat_hidden(current_team_inv);
                    break;
            }

            enemy_mat_invis = create_material("materials/enemy_chams_invis.vmat", evmat_invis.c_str());
            enemy_mat_vis = create_material("materials/enemy_chams_vis.vmat", evmat_vis.c_str());

            team_mat_invis = create_material("materials/team_chams_invis.vmat", tvmat_invis.c_str());
            team_mat_vis = create_material("materials/team_chams_vis.vmat", tvmat_vis.c_str());
        }
    }

    bool Initialize(ID3D11Device* device, ID3D11DeviceContext* context)
    {
        (void)device;
        (void)context;

        if (g_initialized)
            return true;

        MH_STATUS mh_status = MH_Initialize();
        if (mh_status != MH_OK && mh_status != MH_ERROR_ALREADY_INITIALIZED) {
            return false;
        }

        uint8_t* generate_primitives_addr = scan("scenesystem.dll", "48 8B C4 48 89 58 ? 48 89 50 ? 55 56 57 41 54 41 55 41 56 41 57 48 81 EC");
        if (!generate_primitives_addr) {
            return false;
        }

        if (MH_CreateHook(generate_primitives_addr, (LPVOID)&hkGeneratePrimitives, (LPVOID*)&oGeneratePrimitives) != MH_OK) {
            return false;
        }

        if (MH_EnableHook(generate_primitives_addr) != MH_OK) {
            return false;
        }

        initialize_materials();

        g_initialized = true;
        g_hooked = true;
        return true;
    }

    void Shutdown()
    {
        if (g_hooked) {
            uint8_t* generate_primitives_addr = scan("scenesystem.dll", "48 8B C4 48 89 58 ? 48 89 50 ? 55 56 57 41 54 41 55 41 56 41 57 48 81 EC");
            if (generate_primitives_addr) {
                MH_DisableHook(generate_primitives_addr);
                MH_RemoveHook(generate_primitives_addr);
            }
            g_hooked = false;
        }

        g_initialized = false;
    }

    void Update()
    {
        if (!g_initialized)
            return;

        update_materials();
    }

    bool IsReady()
    {
        return g_initialized;
    }
}
