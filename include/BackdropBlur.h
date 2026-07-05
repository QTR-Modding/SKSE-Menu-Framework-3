#pragma once

#include <d3d11.h>

namespace BackdropBlur {
    void Init(ID3D11Device* device, ID3D11DeviceContext* context);
    void BeginWindowCollection();
    void DrawBehindActiveWindows(float strength);
}
