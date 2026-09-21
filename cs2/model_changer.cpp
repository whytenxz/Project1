#include "model_changer.h"
#include "memory.h"
#include "offsets.h"
#include "../nerv/config.hpp"
#include "../Frontend/Framework/MenuFramework.h"

#include <Windows.h>
#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>

namespace model_changer {
    namespace {
        constexpr int kFrameRenderEnd = 6;

        struct ModelEntry {
            std::string name;
            std::string path;
        };

        struct CBufferString {
            int m_nLength = 0;
            int m_nAllocatedSize = static_cast<int>(0x80000000 | 0x40000000 | 8);
            union {
                char* m_pString = nullptr;
                char m_szString[8];
            };
        };

        using SetModelFn = void*(__fastcall*)(void*, const char*);
        using PrecacheFn = void*(__fastcall*)(void*, void*, const char*);
        using BufferInsertFn = const char*(__fastcall*)(void*, int, const char*, int, bool);
        using CreateInterfaceFn = void*(*)(const char*, int*);

        std::vector<ModelEntry> g_models;
        std::vector<std::string> g_nameStorage;
        std::vector<const char*> g_nameCstr;

        SetModelFn g_setModel = nullptr;
        PrecacheFn g_precache = nullptr;
        BufferInsertFn g_bufferInsert = nullptr;
        void* g_resourceSystem = nullptr;

        bool g_initialized = false;
        bool g_pendingApply = false;
        int g_lastSelected = 0;

        void* GetResourceSystem() {
            const HMODULE resourceModule = GetModuleHandleA("resourcesystem.dll");
            if (!resourceModule)
                return nullptr;

            auto createInterface = reinterpret_cast<CreateInterfaceFn>(GetProcAddress(resourceModule, "CreateInterface"));
            if (createInterface)
                return createInterface("ResourceSystem013", nullptr);

            const uintptr_t ciAddr = memory::FindPattern("resourcesystem.dll", "4C 8B 0D ?? ?? ?? ?? 4C 8B D2 4C 8B D9");
            if (!ciAddr)
                return nullptr;

            createInterface = reinterpret_cast<CreateInterfaceFn>(ciAddr);
            return createInterface ? createInterface("ResourceSystem013", nullptr) : nullptr;
        }

        std::string GetModelsRoot() {
            char modulePath[MAX_PATH]{};
            if (GetModuleFileNameA(GetModuleHandleA("client.dll"), modulePath, MAX_PATH)) {
                std::filesystem::path root = std::filesystem::path(modulePath).parent_path().parent_path().parent_path() / "characters" / "models";
                if (std::filesystem::exists(root))
                    return root.string();
            }

            char cwd[MAX_PATH]{};
            if (!GetCurrentDirectoryA(MAX_PATH, cwd))
                return {};

            std::string root = cwd;
            const auto pos = root.find("bin\\win64");
            if (pos != std::string::npos)
                root.replace(pos, 9, "csgo\\characters\\models");

            return root;
        }

        void RebuildNameList() {
            g_nameStorage.clear();
            g_nameCstr.clear();

            for (const auto& model : g_models) {
                g_nameStorage.push_back(model.name);
                g_nameCstr.push_back(g_nameStorage.back().c_str());
            }
        }

        void PrecacheResource(const std::string& path) {
            if (!g_precache || !g_resourceSystem || !g_bufferInsert || path.empty())
                return;

            CBufferString names{};
            g_bufferInsert(&names, 0, path.c_str(), -1, false);
            g_precache(g_resourceSystem, &names, "");
        }

        void* GetLocalPawn() {
            const uintptr_t client = memory::GetModuleBase("client.dll");
            if (!client)
                return nullptr;

            const uintptr_t localPawn = memory::Read<uintptr_t>(client + offsets::client_dll::dwLocalPlayerPawn);
            if (!localPawn)
                return nullptr;

            return reinterpret_cast<void*>(localPawn);
        }

        void ApplySelectedModel() {
            const int selected = g_cfg->model_changer.m_selected;
            if (selected <= 0 || selected >= static_cast<int>(g_models.size()))
                return;

            const std::string& path = g_models[selected].path;
            if (path.empty() || !g_setModel)
                return;

            void* pawn = GetLocalPawn();
            if (!pawn)
                return;

            PrecacheResource(path);
            g_setModel(pawn, path.c_str());
            g_pendingApply = false;
        }
    }

    void Initialize() {
        if (g_initialized)
            return;

        g_setModel = reinterpret_cast<SetModelFn>(
            memory::FindPattern("client.dll", offsets::signatures::set_model));
        g_precache = reinterpret_cast<PrecacheFn>(
            memory::FindPattern("resourcesystem.dll", offsets::signatures::resource_precache));

        const HMODULE tier0 = GetModuleHandleA("tier0.dll");
        if (tier0) {
            g_bufferInsert = reinterpret_cast<BufferInsertFn>(
                GetProcAddress(tier0, "?Insert@CBufferString@@QEAAPEBDHPEBDH_N@Z"));
        }

        g_resourceSystem = GetResourceSystem();

        if (g_models.empty())
            Scan();

        g_initialized = true;

        if (g_cfg->model_changer.m_selected > 0)
            g_pendingApply = true;
    }

    void Scan() {
        g_models.clear();
        g_models.push_back({ "[ OFF ]", "" });

        const std::string root = GetModelsRoot();
        if (!root.empty() && std::filesystem::exists(root)) {
            for (const auto& entry : std::filesystem::recursive_directory_iterator(root)) {
                if (!entry.is_regular_file() || entry.path().extension() != ".vmdl_c")
                    continue;

                const std::string full = entry.path().string();
                const auto marker = full.find("characters\\");
                if (marker == std::string::npos)
                    continue;

                std::string rel = full.substr(marker);
                std::replace(rel.begin(), rel.end(), '\\', '/');

                const auto extPos = rel.find(".vmdl_c");
                if (extPos == std::string::npos)
                    continue;

                rel = rel.substr(0, extPos) + ".vmdl";
                g_models.push_back({ entry.path().stem().string(), rel });
            }
        }

        std::sort(g_models.begin() + 1, g_models.end(), [](const ModelEntry& a, const ModelEntry& b) {
            return a.name < b.name;
        });

        RebuildNameList();

        if (g_cfg->model_changer.m_selected >= static_cast<int>(g_models.size()))
            g_cfg->model_changer.m_selected = 0;
    }

    void RequestApply() {
        g_pendingApply = true;
    }

    void OnLevelInit() {
        if (g_cfg->model_changer.m_selected > 0)
            g_pendingApply = true;
    }

    void OnFrameStage(int stage) {
        if (!g_initialized)
            Initialize();

        if (stage == kFrameRenderEnd && g_pendingApply)
            ApplySelectedModel();
    }

    void PrecacheModelPath(const char* path) {
        if (!path || !path[0])
            return;

        Initialize();
        PrecacheResource(path);
    }

    void SetEntityModel(void* entity, const char* path) {
        if (!entity || !path || !path[0])
            return;

        Initialize();
        if (!g_setModel)
            return;

        PrecacheResource(path);
        g_setModel(entity, path);
    }

    void DrawMenu() {
        using namespace IdaLovesMe;

        Initialize();

        if (ui::Button("Refresh models"))
            Scan();

        if (g_nameCstr.empty())
            RebuildNameList();

        if (g_nameCstr.empty()) {
            ui::Label("No custom models found");
            ui::Label("Place .vmdl_c under csgo/characters/models");
            return;
        }

        int selected = g_cfg->model_changer.m_selected;
        if (selected < 0 || selected >= static_cast<int>(g_nameCstr.size()))
            selected = 0;

        if (ui::SingleSelect("Custom model", &selected, g_nameCstr)) {
            g_cfg->model_changer.m_selected = selected;
            if (selected != g_lastSelected) {
                g_lastSelected = selected;
                if (selected != 0)
                    RequestApply();
            }
        }
    }
}
