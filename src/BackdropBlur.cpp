#include "BackdropBlur.h"

#include <algorithm>
#include <d3dcompiler.h>
#include <imgui.h>
#include <imgui_internal.h>
#include <unordered_set>

namespace {
    struct BlurConstants {
        float texelSize[2];
        float direction[2];
        float radius;
        float padding[3];
    };

    ID3D11Device* device = nullptr;
    ID3D11DeviceContext* context = nullptr;
    ID3D11Texture2D* framebufferCopy = nullptr;
    ID3D11ShaderResourceView* framebufferView = nullptr;
    ID3D11Texture2D* intermediateTexture = nullptr;
    ID3D11ShaderResourceView* intermediateView = nullptr;
    ID3D11RenderTargetView* intermediateTarget = nullptr;
    ID3D11VertexShader* vertexShader = nullptr;
    ID3D11PixelShader* pixelShader = nullptr;
    ID3D11Buffer* constantBuffer = nullptr;
    ID3D11SamplerState* sampler = nullptr;
    UINT textureWidth = 0;
    UINT textureHeight = 0;
    DXGI_FORMAT textureFormat = DXGI_FORMAT_UNKNOWN;
    float blurStrength = 0.0f;
    std::unordered_set<ImGuiWindow*> windowsActiveBeforeCollection;

    template <class T>
    void Release(T*& resource) {
        if (resource) {
            resource->Release();
            resource = nullptr;
        }
    }

    bool CreateShaders() {
        if (vertexShader && pixelShader && constantBuffer && sampler) {
            return true;
        }

        static constexpr char vertexSource[] = R"(
            struct Output { float4 position : SV_POSITION; float2 uv : TEXCOORD0; };
            Output main(uint id : SV_VertexID) {
                Output output;
                output.uv = float2((id << 1) & 2, id & 2);
                output.position = float4(output.uv * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);
                return output;
            }
        )";

        static constexpr char pixelSource[] = R"(
            Texture2D sourceTexture : register(t0);
            SamplerState sourceSampler : register(s0);
            cbuffer BlurConstants : register(b0) {
                float2 texelSize;
                float2 direction;
                float radius;
                float3 padding;
            };

            float4 main(float4 position : SV_POSITION, float2 uv : TEXCOORD0) : SV_TARGET {
                // Keep samples close together at high strengths. Widely spaced taps
                // are cheap, but become visible as repeated/segmented imagery.
                static const float weights[13] = {
                    1.000000, 0.986207, 0.945959, 0.882497, 0.800737, 0.706648, 0.606531,
                    0.506336, 0.411112, 0.324652, 0.249352, 0.186270, 0.135335
                };
                float2 axis = direction * texelSize * radius;
                float4 color = sourceTexture.Sample(sourceSampler, uv) * weights[0];
                [unroll] for (int tap = 1; tap <= 12; ++tap) {
                    float2 sampleOffset = axis * (tap / 6.0);
                    color += sourceTexture.Sample(sourceSampler, uv + sampleOffset) * weights[tap];
                    color += sourceTexture.Sample(sourceSampler, uv - sampleOffset) * weights[tap];
                }
                return color / 14.483272;
            }
        )";

        ID3DBlob* vertexBlob = nullptr;
        ID3DBlob* pixelBlob = nullptr;
        ID3DBlob* errors = nullptr;
        HRESULT result = D3DCompile(vertexSource, sizeof(vertexSource), nullptr, nullptr, nullptr, "main", "vs_5_0",
                                    0, 0, &vertexBlob, &errors);
        Release(errors);
        if (FAILED(result)) {
            Release(vertexBlob);
            return false;
        }

        result = D3DCompile(pixelSource, sizeof(pixelSource), nullptr, nullptr, nullptr, "main", "ps_5_0",
                            0, 0, &pixelBlob, &errors);
        Release(errors);
        if (FAILED(result)) {
            Release(vertexBlob);
            Release(pixelBlob);
            return false;
        }

        result = device->CreateVertexShader(vertexBlob->GetBufferPointer(), vertexBlob->GetBufferSize(), nullptr,
                                            &vertexShader);
        if (SUCCEEDED(result)) {
            result = device->CreatePixelShader(pixelBlob->GetBufferPointer(), pixelBlob->GetBufferSize(), nullptr,
                                               &pixelShader);
        }
        Release(vertexBlob);
        Release(pixelBlob);
        if (FAILED(result)) {
            return false;
        }

        D3D11_BUFFER_DESC bufferDesc{};
        bufferDesc.ByteWidth = sizeof(BlurConstants);
        bufferDesc.Usage = D3D11_USAGE_DYNAMIC;
        bufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        if (FAILED(device->CreateBuffer(&bufferDesc, nullptr, &constantBuffer))) {
            return false;
        }

        D3D11_SAMPLER_DESC samplerDesc{};
        samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
        samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
        samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
        samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
        return SUCCEEDED(device->CreateSamplerState(&samplerDesc, &sampler));
    }

    bool UpdateFramebufferCopy(ID3D11Texture2D* source) {
        D3D11_TEXTURE2D_DESC sourceDesc{};
        source->GetDesc(&sourceDesc);
        if (sourceDesc.SampleDesc.Count != 1) {
            return false;
        }

        if (framebufferCopy && textureWidth == sourceDesc.Width && textureHeight == sourceDesc.Height &&
            textureFormat == sourceDesc.Format) {
            return true;
        }

        Release(framebufferView);
        Release(framebufferCopy);
        Release(intermediateTarget);
        Release(intermediateView);
        Release(intermediateTexture);

        D3D11_TEXTURE2D_DESC copyDesc = sourceDesc;
        copyDesc.Usage = D3D11_USAGE_DEFAULT;
        copyDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        copyDesc.CPUAccessFlags = 0;
        copyDesc.MiscFlags = 0;
        if (FAILED(device->CreateTexture2D(&copyDesc, nullptr, &framebufferCopy)) ||
            FAILED(device->CreateShaderResourceView(framebufferCopy, nullptr, &framebufferView))) {
            Release(framebufferView);
            Release(framebufferCopy);
            return false;
        }

        D3D11_TEXTURE2D_DESC intermediateDesc = copyDesc;
        intermediateDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
        if (FAILED(device->CreateTexture2D(&intermediateDesc, nullptr, &intermediateTexture)) ||
            FAILED(device->CreateShaderResourceView(intermediateTexture, nullptr, &intermediateView)) ||
            FAILED(device->CreateRenderTargetView(intermediateTexture, nullptr, &intermediateTarget))) {
            Release(intermediateTarget);
            Release(intermediateView);
            Release(intermediateTexture);
            Release(framebufferView);
            Release(framebufferCopy);
            return false;
        }

        textureWidth = sourceDesc.Width;
        textureHeight = sourceDesc.Height;
        textureFormat = sourceDesc.Format;
        return true;
    }

    bool UpdateConstants(float directionX, float directionY) {
        D3D11_MAPPED_SUBRESOURCE mapped{};
        if (FAILED(context->Map(constantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) {
            return false;
        }
        auto* constants = static_cast<BlurConstants*>(mapped.pData);
        constants->texelSize[0] = 1.0f / static_cast<float>(textureWidth);
        constants->texelSize[1] = 1.0f / static_cast<float>(textureHeight);
        constants->direction[0] = directionX;
        constants->direction[1] = directionY;
        constants->radius = blurStrength;
        constants->padding[0] = constants->padding[1] = constants->padding[2] = 0.0f;
        context->Unmap(constantBuffer, 0);
        return true;
    }

    void RenderBlur(const ImDrawList*, const ImDrawCmd* command) {
        ID3D11RenderTargetView* renderTarget = nullptr;
        context->OMGetRenderTargets(1, &renderTarget, nullptr);
        if (!renderTarget) {
            return;
        }

        ID3D11Resource* resource = nullptr;
        ID3D11Texture2D* source = nullptr;
        renderTarget->GetResource(&resource);
        const HRESULT queryResult = resource->QueryInterface(__uuidof(ID3D11Texture2D),
                                                             reinterpret_cast<void**>(&source));
        Release(resource);
        if (FAILED(queryResult) || !UpdateFramebufferCopy(source) || !CreateShaders()) {
            Release(source);
            Release(renderTarget);
            return;
        }

        ID3D11ShaderResourceView* emptyView = nullptr;
        context->PSSetShaderResources(0, 1, &emptyView);
        context->CopyResource(framebufferCopy, source);
        Release(source);

        const D3D11_RECT scissor{
            static_cast<LONG>(command->ClipRect.x), static_cast<LONG>(command->ClipRect.y),
            static_cast<LONG>(command->ClipRect.z), static_cast<LONG>(command->ClipRect.w)};
        const LONG margin = static_cast<LONG>(blurStrength * 2.0f + 2.0f);
        const D3D11_RECT expandedScissor{
            std::max<LONG>(0, scissor.left - margin), std::max<LONG>(0, scissor.top - margin),
            std::min<LONG>(textureWidth, scissor.right + margin),
            std::min<LONG>(textureHeight, scissor.bottom + margin)};

        context->IASetInputLayout(nullptr);
        context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        context->VSSetShader(vertexShader, nullptr, 0);
        context->PSSetShader(pixelShader, nullptr, 0);
        context->PSSetConstantBuffers(0, 1, &constantBuffer);
        context->PSSetSamplers(0, 1, &sampler);
        context->OMSetBlendState(nullptr, nullptr, 0xffffffff);

        // Horizontal pass. Only process the window plus the kernel margin needed by the vertical pass.
        context->OMSetRenderTargets(1, &intermediateTarget, nullptr);
        context->RSSetScissorRects(1, &expandedScissor);
        if (!UpdateConstants(1.0f, 0.0f)) {
            context->OMSetRenderTargets(1, &renderTarget, nullptr);
            Release(renderTarget);
            return;
        }
        context->PSSetShaderResources(0, 1, &framebufferView);
        context->Draw(3, 0);
        context->PSSetShaderResources(0, 1, &emptyView);

        // Vertical pass back into the game target, clipped to the exact window rectangle.
        context->OMSetRenderTargets(1, &renderTarget, nullptr);
        context->RSSetScissorRects(1, &scissor);
        if (UpdateConstants(0.0f, 1.0f)) {
            context->PSSetShaderResources(0, 1, &intermediateView);
            context->Draw(3, 0);
            context->PSSetShaderResources(0, 1, &emptyView);
        }
        Release(renderTarget);
    }
}

void BackdropBlur::Init(ID3D11Device* newDevice, ID3D11DeviceContext* newContext) {
    device = newDevice;
    context = newContext;
}

void BackdropBlur::BeginWindowCollection() {
    windowsActiveBeforeCollection.clear();
    ImGuiContext& imgui = *ImGui::GetCurrentContext();
    for (ImGuiWindow* window : imgui.Windows) {
        if (window->LastFrameActive == imgui.FrameCount) {
            windowsActiveBeforeCollection.insert(window);
        }
    }
}

void BackdropBlur::DrawBehindActiveWindows(float strength) {
    if (!device || !context || strength <= 0.0f) {
        return;
    }

    blurStrength = std::clamp(strength, 0.25f, 16.0f);
    ImGuiContext& imgui = *ImGui::GetCurrentContext();
    ImDrawList* background = ImGui::GetBackgroundDrawList();
    const ImRect viewportRect{imgui.Viewports[0]->Pos, imgui.Viewports[0]->Pos + imgui.Viewports[0]->Size};

    for (ImGuiWindow* window : imgui.Windows) {
        if (windowsActiveBeforeCollection.contains(window) || window->LastFrameActive != imgui.FrameCount ||
            window->Hidden || window->Collapsed ||
            (window->Flags & (ImGuiWindowFlags_ChildWindow | ImGuiWindowFlags_NoBackground))) {
            continue;
        }

        ImRect rectangle = window->Rect();
        rectangle.ClipWith(viewportRect);
        if (rectangle.IsInverted()) {
            continue;
        }

        background->PushClipRect(rectangle.Min, rectangle.Max, true);
        background->AddCallback(RenderBlur, nullptr);
        background->AddCallback(ImDrawCallback_ResetRenderState, nullptr);
        background->PopClipRect();
    }
}
