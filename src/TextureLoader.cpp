#include "TextureLoader.h"



#include <codecvt>
#include <cstdint>
#include <format>
#include <locale>
#include <string>
#include <vector>

#include "DirectXTK/DDSTextureLoader.h"
#include "DirectXTK/WICTextureLoader.h"
#include "nanosvg.h"
#include "nanosvgrast.h"

namespace File {
    inline bool Exists(const wchar_t* filename) {
        std::ifstream file(filename);
        return file.good();
    }
}

ImTextureID TextureLoader::LoadDDS(std::string imagePath) {
    auto textureId = TextureLoaderImpl::LoadTextureFromDDSFile(imagePath);
    return reinterpret_cast<ImTextureID>(textureId);
}

ImTextureID TextureLoader::LoadWIC(std::string imagePath) {
    auto textureId = TextureLoaderImpl::LoadTextureFromWICFile(imagePath);
    return reinterpret_cast<ImTextureID>(textureId);
}

ImTextureID TextureLoader::LoadSVG(std::string imagePath, ImVec2 size) {
    auto textureId = TextureLoaderImpl::LoadTextureFromSVGFile(imagePath, size);
    return reinterpret_cast<ImTextureID>(textureId);
}


const wchar_t* convertToWChar(const std::string& str) {
    static std::vector<wchar_t> wideStr;
    wideStr.clear();

    std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
    std::wstring wide = converter.from_bytes(str);
    wideStr.assign(wide.begin(), wide.end());
    wideStr.push_back(L'\0');
    return wideStr.data();
}

bool endsWith(const std::string& str, const std::string& suffix) {
    if (suffix.size() > str.size()) return false;
    return str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0;
}

ImTextureID TextureLoader::LoadTextureAny(std::string imagePath, ImVec2 size) {
    if (endsWith(imagePath, ".dds")) {
        return LoadDDS(imagePath);
    }
    if (endsWith(imagePath, ".svg")) {
        return LoadSVG(imagePath, size);
    }
    return LoadWIC(imagePath);
}

void TextureLoader::DisposeTexture(std::string path) {
    auto it = textures.find(path);
    if (it != textures.end()) {
        if (it->second) {
            reinterpret_cast<ID3D11ShaderResourceView*>(it->second)->Release();
        }
        textures.erase(it);
    }
}

ImTextureID TextureLoader::GetTexture(std::string texturePath, ImVec2 size) {
    std::string textureKey = texturePath;

    constexpr float snapFactor = 8.f;

    auto rendersize = size;

    size.x = std::ceil(size.x / snapFactor) * snapFactor;
    size.y = std::ceil(size.y / snapFactor) * snapFactor;

    if (endsWith(textureKey, ".svg")) {
        textureKey = std::format("{}-{}-{}", texturePath, (int)size.x, (int)size.y);
    }

    auto it = textures.find(textureKey);

    ImTextureID textureId;

    if (it != textures.end()) {
        textureId = it->second;
    } else {
        textureId = LoadTextureAny(texturePath, size);
        if (textureId == 0) {
            return 0;
        }
        textures[textureKey] = textureId;
    }

    return textureId;
}

void TextureLoader::Init(ID3D11Device* device, ID3D11DeviceContext* context) {
    TextureLoaderImpl::Init(device, context);
}


void TextureLoader::TextureLoaderImpl::Init(ID3D11Device* a_device, ID3D11DeviceContext* a_context) {
    device = a_device;
    context = a_context;
}

ID3D11ShaderResourceView* TextureLoader::TextureLoaderImpl::LoadTextureFromDDSFile(std::string path) {
    if (!device || !context) return NULL;
    auto wpath = convertToWChar(path);

    if (!File::Exists(wpath)) {
        return NULL;
    }

    ID3D11ShaderResourceView* texture = nullptr;
    const HRESULT result = DirectX::CreateDDSTextureFromFile(device, context, wpath, nullptr, &texture);
    if (FAILED(result)) {
        logger::warn("Could not load DDS texture '{}' (HRESULT: 0x{:08X})", path,
                     static_cast<std::uint32_t>(result));
        return nullptr;
    }

    return texture;
}

ID3D11ShaderResourceView* TextureLoader::TextureLoaderImpl::LoadTextureFromWICFile(std::string path) {
    if (!device || !context) return NULL;
    auto wpath = convertToWChar(path);

    if (!File::Exists(wpath)) {
        return NULL;
    }

    ID3D11ShaderResourceView* texture = nullptr;
    const HRESULT result = DirectX::CreateWICTextureFromFile(device, context, wpath, nullptr, &texture);
    if (FAILED(result)) {
        logger::warn("Could not load WIC texture '{}' (HRESULT: 0x{:08X})", path,
                     static_cast<std::uint32_t>(result));
        return nullptr;
    }

    return texture;
}

ID3D11ShaderResourceView* TextureLoader::TextureLoaderImpl::LoadTextureFromSVGFile(std::string path, ImVec2 size) {
    NSVGimage* image = nsvgParseFromFile(path.c_str(), "px", 96);
    return ReadSVG(image, size);

}



ID3D11ShaderResourceView* TextureLoader::TextureLoaderImpl::ReadSVG(NSVGimage* image, ImVec2 size) { 
    if (!image) {
        return nullptr;
    }

    int width = (int)size.x;
    int height = (int)size.y;

    NSVGrasterizer* rast = nsvgCreateRasterizer();
    unsigned char* img = (unsigned char*)malloc(width * height * 4);

    if (!img) {
        nsvgDeleteRasterizer(rast);
        nsvgDelete(image);
        return nullptr;
    }

    float scaleX = size.x / image->width;
    float scaleY = size.y / image->height;
    float scale = std::min(scaleX, scaleY);

    float offsetX = (width - image->width * scale) / 2;
    float offsetY = (height - image->height * scale) / 2;

    nsvgRasterize(rast, image, offsetX, offsetY, scale, img, width, height, width * 4);
    nsvgDeleteRasterizer(rast);
    nsvgDelete(image);

    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA subResource = {};
    subResource.pSysMem = img;
    subResource.SysMemPitch = width * 4;

    ID3D11Texture2D* pTexture = nullptr;
    HRESULT hr = device->CreateTexture2D(&desc, &subResource, &pTexture);
    if (FAILED(hr)) {
        free(img);
        return nullptr;
    }

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = desc.Format;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;

    ID3D11ShaderResourceView* textureView = nullptr;
    hr = device->CreateShaderResourceView(pTexture, &srvDesc, &textureView);
    pTexture->Release();
    free(img);

    if (FAILED(hr)) {
        return nullptr;
    }
    return textureView;
}
