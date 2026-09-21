#pragma once

#include <d3d11.h>

namespace chams {
    bool Initialize(ID3D11Device* device, ID3D11DeviceContext* context);
    void Shutdown();
    void Update();
    bool IsReady();
}
