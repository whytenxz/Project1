#include "SteamAvatars.h"

#include <chrono>
#include <cstring>
#include <unordered_map>
#include <vector>


namespace SteamAvatars {
    namespace {
        struct CSteamID {
            uint64_t value = 0;
        };

        struct CacheEntry {
            LPDIRECT3DTEXTURE9 texture = nullptr;
            AvatarState state = AvatarState::None;
            std::chrono::steady_clock::time_point requestedAt{};
            bool userInfoRequested = false;
        };

        ID3D11Device* g_device = nullptr;
        void* g_steamClient = nullptr;
        void* g_steamFriends = nullptr;
        void* g_steamUtils = nullptr;
        uint32_t g_steamPipe = 0;
        uint32_t g_steamUser = 0;

        std::unordered_map<uint64_t, CacheEntry> g_cache;

        template<typename T, typename... Args>
        T VCall(void* instance, size_t index, Args... args) {
            if (!instance)
                return T{};
            void** vtable = *reinterpret_cast<void***>(instance);
            using Fn = T(__fastcall*)(void*, Args...);
            return reinterpret_cast<Fn>(vtable[index])(instance, args...);
        }

        using CreateInterfaceFn = void* (*)(const char*, int*);

        void* CreateSteamInterface(const char* version) {
            static CreateInterfaceFn createInterface = nullptr;
            if (!createInterface) {
                HMODULE steamApi = GetModuleHandleA("steam_api64.dll");
                if (steamApi)
                    createInterface = reinterpret_cast<CreateInterfaceFn>(GetProcAddress(steamApi, "SteamInternal_CreateInterface"));
                if (!createInterface) {
                    HMODULE steamClient = GetModuleHandleA("steamclient64.dll");
                    if (steamClient)
                        createInterface = reinterpret_cast<CreateInterfaceFn>(GetProcAddress(steamClient, "CreateInterface"));
                }
            }
            if (!createInterface)
                return nullptr;
            return createInterface(version, nullptr);
        }

        bool InitSteamInterfaces() {
            if (g_steamFriends && g_steamUtils)
                return true;

            static const char* kClientVersions[] = { "SteamClient021", "SteamClient020", "SteamClient019" };
            static const char* kFriendsVersions[] = { "SteamFriends017", "SteamFriends016", "SteamFriends015" };
            static const char* kUtilsVersions[] = { "SteamUtils010", "SteamUtils009" };

            for (const char* clientVersion : kClientVersions) {
                g_steamClient = CreateSteamInterface(clientVersion);
                if (!g_steamClient)
                    continue;

                g_steamPipe = VCall<uint32_t>(g_steamClient, 0);
                g_steamUser = VCall<uint32_t>(g_steamClient, 2, g_steamPipe);
                if (!g_steamPipe || !g_steamUser)
                    continue;

                for (const char* friendsVersion : kFriendsVersions) {
                    g_steamFriends = VCall<void*>(g_steamClient, 8, g_steamUser, g_steamPipe, friendsVersion);
                    if (g_steamFriends)
                        break;
                }

                for (const char* utilsVersion : kUtilsVersions) {
                    g_steamUtils = VCall<void*>(g_steamClient, 9, g_steamUser, g_steamPipe, utilsVersion);
                    if (g_steamUtils)
                        break;
                }

                if (g_steamFriends && g_steamUtils)
                    return true;
            }

            g_steamClient = nullptr;
            g_steamFriends = nullptr;
            g_steamUtils = nullptr;
            g_steamPipe = 0;
            g_steamUser = 0;
            return false;
        }

        bool LoadTextureFromRgba(ID3D11Device* device, const uint8_t* pixels, uint32_t width, uint32_t height, LPDIRECT3DTEXTURE9* outSrv) {
            if (!device || !pixels || !width || !height || !outSrv)
                return false;

            D3D11_TEXTURE2D_DESC desc{};
            desc.Width = width;
            desc.Height = height;
            desc.MipLevels = 1;
            desc.ArraySize = 1;
            desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            desc.SampleDesc.Count = 1;
            desc.Usage = D3D11_USAGE_DEFAULT;
            desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

            D3D11_SUBRESOURCE_DATA sub{};
            sub.pSysMem = pixels;
            sub.SysMemPitch = width * 4;

            ID3D11Texture2D* texture = nullptr;
            if (FAILED(device->CreateTexture2D(&desc, &sub, &texture)))
                return false;

            device->CreateShaderResourceView(texture, nullptr, reinterpret_cast<ID3D11ShaderResourceView**>(outSrv));
            texture->Release();
            return *outSrv != nullptr;
        }

        void RequestUserInfo(uint64_t steamId, CacheEntry& entry) {
            if (!g_steamFriends || entry.userInfoRequested)
                return;

            const CSteamID id{ steamId };
            VCall<bool>(g_steamFriends, 21, id, false);
            entry.userInfoRequested = true;
        }

        bool TryCreateAvatarTexture(uint64_t steamId, CacheEntry& entry) {
            if (!g_device || !g_steamFriends || !g_steamUtils)
                return false;

            const CSteamID id{ steamId };
            const int image = VCall<int>(g_steamFriends, 34, id);
            if (image == 0) {
                entry.state = AvatarState::Failed;
                return true;
            }
            if (image < 0)
                return false;

            uint32_t width = 0, height = 0;
            if (!VCall<bool>(g_steamUtils, 4, image, &width, &height) || width == 0 || height == 0)
                return false;

            std::vector<uint8_t> rgba(width * height * 4);
            if (!VCall<bool>(g_steamUtils, 5, image, rgba.data(), static_cast<int>(width), static_cast<int>(height)))
                return false;

            if (entry.texture) {
                entry.texture->Release();
                entry.texture = nullptr;
            }

            if (LoadTextureFromRgba(g_device, rgba.data(), width, height, &entry.texture)) {
                entry.state = AvatarState::Ready;
                return true;
            }

            entry.state = AvatarState::Failed;
            return true;
        }

        bool IsTimedOut(const CacheEntry& entry) {
            using namespace std::chrono;
            return entry.state == AvatarState::Loading &&
                steady_clock::now() - entry.requestedAt > seconds(5);
        }
    }

    void Init(ID3D11Device* device) {
        g_device = device;
        InitSteamInterfaces();
    }

    void Shutdown() {
        for (auto& pair : g_cache) {
            if (pair.second.texture) {
                pair.second.texture->Release();
                pair.second.texture = nullptr;
            }
        }
        g_cache.clear();
        g_steamClient = nullptr;
        g_steamFriends = nullptr;
        g_steamUtils = nullptr;
        g_steamPipe = 0;
        g_steamUser = 0;
        g_device = nullptr;
    }

    void Request(uint64_t steamId) {
        if (steamId < 76561197960265728ull)
            return;

        CacheEntry& entry = g_cache[steamId];
        if (entry.state == AvatarState::Loading || entry.state == AvatarState::Ready)
            return;

        entry.state = AvatarState::Loading;
        entry.requestedAt = std::chrono::steady_clock::now();
        entry.userInfoRequested = false;
    }

    void Tick() {
        if (!g_device)
            return;

        if (!InitSteamInterfaces()) {
            for (auto& pair : g_cache) {
                if (pair.second.state == AvatarState::Loading)
                    pair.second.state = AvatarState::Failed;
            }
            return;
        }

        int processed = 0;
        for (auto& pair : g_cache) {
            if (processed >= 1)
                break;

            CacheEntry& entry = pair.second;
            if (entry.state != AvatarState::Loading)
                continue;

            if (IsTimedOut(entry)) {
                entry.state = AvatarState::Failed;
                continue;
            }

            RequestUserInfo(pair.first, entry);
            if (TryCreateAvatarTexture(pair.first, entry))
                ++processed;
        }
    }

    AvatarState GetState(uint64_t steamId) {
        const auto it = g_cache.find(steamId);
        if (it == g_cache.end())
            return AvatarState::None;
        return it->second.state;
    }

    LPDIRECT3DTEXTURE9 GetTexture(uint64_t steamId) {
        const auto it = g_cache.find(steamId);
        if (it == g_cache.end() || it->second.state != AvatarState::Ready)
            return nullptr;
        return it->second.texture;
    }
}
