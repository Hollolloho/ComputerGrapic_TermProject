#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <wincodec.h>
#include <wrl/client.h>

#include "Texture.h"
#include <algorithm>
#include <iostream>
#include <vector>

using Microsoft::WRL::ComPtr;

namespace
{
struct ComContext
{
    HRESULT status = CoInitializeEx(nullptr, COINIT_MULTITHREADED);

    ~ComContext()
    {
        if (SUCCEEDED(status))
        {
            CoUninitialize();
        }
    }
};
}

GLuint LoadPngTexture(const std::filesystem::path& path, bool srgb, GLint wrapMode)
{
    ComContext com;
    if (FAILED(com.status) && com.status != RPC_E_CHANGED_MODE)
        return 0;
    ComPtr<IWICImagingFactory> factory;
    if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory))))
    {
        std::cerr << "Cannot initialize PNG decoder: " << path << '\n';
        return 0;
    }
    ComPtr<IWICBitmapDecoder> decoder;
    ComPtr<IWICBitmapFrameDecode> frame;
    ComPtr<IWICFormatConverter> converter;
    HRESULT hr = factory->CreateDecoderFromFilename(path.c_str(), nullptr, GENERIC_READ,
                                                    WICDecodeMetadataCacheOnLoad, &decoder);
    if (SUCCEEDED(hr))
    {
        hr = decoder->GetFrame(0, &frame);
    }
    if (SUCCEEDED(hr))
    {
        hr = factory->CreateFormatConverter(&converter);
    }
    if (SUCCEEDED(hr))
    {
        hr = converter->Initialize(frame.Get(), GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone,
                                   nullptr, 0, WICBitmapPaletteTypeCustom);
    }
    UINT width{}, height{};
    if (SUCCEEDED(hr))
    {
        hr = converter->GetSize(&width, &height);
    }
    if (FAILED(hr) || !width || !height || width > 8192 || height > 8192)
    {
        std::cerr << "Cannot decode PNG: " << path << " HRESULT=" << std::hex << hr << std::dec
                  << '\n';
        return 0;
    }
    const UINT stride = width * 4;
    std::vector<unsigned char> pixels(static_cast<std::size_t>(stride) * height);
    hr = converter->CopyPixels(nullptr, stride, static_cast<UINT>(pixels.size()), pixels.data());
    if (FAILED(hr))
    {
        std::cerr << "Cannot read PNG pixels: " << path << '\n';
        return 0;
    }
    // PNG는 위에서 아래로 읽고 OBJ의 UV 원점은 왼쪽 아래이므로 행을 위아래로 교환한다.
    for (UINT y = 0; y < height / 2; ++y)
    {
        std::swap_ranges(pixels.begin() + static_cast<std::size_t>(y) * stride,
                         pixels.begin() + static_cast<std::size_t>(y + 1) * stride,
                         pixels.begin() + static_cast<std::size_t>(height - 1 - y) * stride);
    }
    GLuint texture{};
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, srgb ? GL_SRGB8_ALPHA8 : GL_RGBA8, static_cast<GLsizei>(width),
                 static_cast<GLsizei>(height), 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrapMode);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrapMode);
    GLint uploadedWidth{};
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &uploadedWidth);
    if (uploadedWidth != static_cast<GLint>(width))
    {
        std::cerr << "Texture upload failed: " << path << '\n';
        glDeleteTextures(1, &texture);
        return 0;
    }
    return texture;
}
