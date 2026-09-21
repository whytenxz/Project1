#pragma once
#include "../../Backend/Globalincludes.h"
#include <cstdint>

namespace SteamAvatars {
    enum class AvatarState {
        None,
        Loading,
        Ready,
        Failed
    };

    void Init(ID3D11Device* device);
    void Shutdown();
    void Tick();
    void Request(uint64_t steamId);
    AvatarState GetState(uint64_t steamId);
    LPDIRECT3DTEXTURE9 GetTexture(uint64_t steamId);
}
