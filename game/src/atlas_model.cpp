// 보스·무기의 메시와 아틀라스 텍스처를 GPU에 올리고 그린다.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <wincodec.h>
#include <wrl/client.h>

#include "atlas_model.h"
#include "model_loader.h"

#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <fstream>
#include <iostream>
#include <sstream>

namespace aegis
{
namespace
{
using Microsoft::WRL::ComPtr;

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

GLuint CompileWeaponShader(GLenum kind, const std::filesystem::path& path)
{
    std::ifstream file(path);
    if (!file)
    {
        std::cerr << "Cannot read weapon shader: " << path << '\n';
        return 0;
    }
    std::ostringstream buffer;
    buffer << file.rdbuf();
    const std::string source = buffer.str();
    const char* text = source.c_str();
    GLuint shader = glCreateShader(kind);
    glShaderSource(shader, 1, &text, nullptr);
    glCompileShader(shader);
    GLint success{};
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        char log[2048]{};
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        std::cerr << path << ": " << log << '\n';
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

GLuint LoadPng(IWICImagingFactory* factory, const std::filesystem::path& path, bool srgb)
{
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
        std::cerr << "Cannot decode weapon PNG: " << path << " HRESULT=" << std::hex << hr << std::dec
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
    // WIC is top-down; OBJ UVs use a bottom-left origin. Keep normal-map +Y.
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
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    GLint uploadedWidth{};
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &uploadedWidth);
    if (uploadedWidth != static_cast<GLint>(width))
    {
        std::cerr << "Weapon texture upload failed: " << path << '\n';
        glDeleteTextures(1, &texture);
        return 0;
    }
    return texture;
}
} // namespace

bool AtlasModel::Init(const std::filesystem::path& root, const std::filesystem::path& runtime,
                      std::span<const AtlasPart> parts, std::span<const AtlasTexture> textureSpecs,
                      float emission)
{
    Clear();
    if (textureSpecs.size() != 5 || parts.empty())
    {
        return false;
    }
    baseEmission = emission;
    const auto shaders = root / "shaders";
    const GLuint vertex = CompileWeaponShader(GL_VERTEX_SHADER, shaders / "weapon.vert");
    const GLuint fragment = CompileWeaponShader(GL_FRAGMENT_SHADER, shaders / "weapon.frag");
    if (!vertex || !fragment)
    {
        if (vertex)
        {
            glDeleteShader(vertex);
        }
        if (fragment)
        {
            glDeleteShader(fragment);
        }
        return false;
    }
    program = glCreateProgram();
    glAttachShader(program, vertex);
    glAttachShader(program, fragment);
    glLinkProgram(program);
    glDeleteShader(vertex);
    glDeleteShader(fragment);
    GLint linked{};
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (!linked)
    {
        char log[2048]{};
        glGetProgramInfoLog(program, sizeof(log), nullptr, log);
        std::cerr << "Weapon program: " << log << '\n';
        Clear();
        return false;
    }
    transformLocation = glGetUniformLocation(program, "transform");
    modelLocation = glGetUniformLocation(program, "model");
    shotLocation = glGetUniformLocation(program, "shot");
    if (transformLocation < 0 || modelLocation < 0 || shotLocation < 0)
    {
        Clear();
        return false;
    }
    ComContext com;
    if (FAILED(com.status) && com.status != RPC_E_CHANGED_MODE)
    {
        std::cerr << "Cannot initialize WIC COM apartment\n";
        Clear();
        return false;
    }
    ComPtr<IWICImagingFactory> factory;
    const HRESULT hr =
        CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory));
    if (FAILED(hr))
    {
        std::cerr << "Cannot initialize Windows PNG decoder\n";
        Clear();
        return false;
    }
    glUseProgram(program);
    emissionLocation = glGetUniformLocation(program, "emissionMultiplier");
    if (emissionLocation < 0)
    {
        Clear();
        return false;
    }
    glUniform1f(emissionLocation, baseEmission);
    for (std::size_t i = 0; i < textures.size(); ++i)
    {
        const auto& spec = textureSpecs[i];
        textures[i] = LoadPng(factory.Get(), runtime / spec.file, spec.srgb);
        if (!textures[i])
        {
            Clear();
            return false;
        }
        const std::string sampler = std::string(spec.name) + "Map";
        const GLint location = glGetUniformLocation(program, sampler.c_str());
        if (location < 0)
        {
            std::cerr << "Missing sampler: " << sampler << '\n';
            Clear();
            return false;
        }
        glUniform1i(location, static_cast<GLint>(i));
    }
    std::size_t totalTriangles = 0;
    for (std::size_t partIndex = 0; partIndex < parts.size(); ++partIndex)
    {
        const auto& part = parts[partIndex];
        ModelData model;
        std::string error;
        if (!LoadModel(runtime / part.mesh, model, error))
        {
            std::cerr << error << '\n';
            Clear();
            return false;
        }
        std::size_t partTriangles = 0;
        for (const auto& mesh : model.meshes)
        {
            if (!mesh.hasUvs || mesh.indices.empty())
            {
                std::cerr << "Weapon mesh has no UV/triangles: " << part.mesh << '\n';
                Clear();
                return false;
            }
            auto& gpu = meshes.emplace_back();
            gpu.partIndex = partIndex;
            glGenVertexArrays(1, &gpu.vao);
            glGenBuffers(1, &gpu.vbo);
            glGenBuffers(1, &gpu.ebo);
            glBindVertexArray(gpu.vao);
            glBindBuffer(GL_ARRAY_BUFFER, gpu.vbo);
            glBufferData(GL_ARRAY_BUFFER, mesh.vertices.size() * sizeof(ModelVertex), mesh.vertices.data(),
                         GL_STATIC_DRAW);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gpu.ebo);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, mesh.indices.size() * sizeof(unsigned int),
                         mesh.indices.data(), GL_STATIC_DRAW);
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(ModelVertex),
                                  reinterpret_cast<void*>(offsetof(ModelVertex, position)));
            glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(ModelVertex),
                                  reinterpret_cast<void*>(offsetof(ModelVertex, normal)));
            glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(ModelVertex),
                                  reinterpret_cast<void*>(offsetof(ModelVertex, uv)));
            glEnableVertexAttribArray(0);
            glEnableVertexAttribArray(1);
            glEnableVertexAttribArray(2);
            gpu.indices = static_cast<GLsizei>(mesh.indices.size());
            partTriangles += mesh.indices.size() / 3;
        }
        if (partTriangles != static_cast<std::size_t>(part.triangles))
        {
            std::cerr << "Weapon layout is stale: " << part.mesh << '\n';
            Clear();
            return false;
        }
        totalTriangles += partTriangles;
    }
    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glUseProgram(0);
    std::cout << "Atlas model: parts=" << parts.size() << " triangles=" << totalTriangles
              << " textures=" << textures.size() << '\n';
    return true;
}

void AtlasModel::Draw(const glm::mat4& projection, const glm::mat4& view,
                      std::span<const glm::mat4> partMatrices, float glow,
                      std::span<const float> emissionScales) const
{
    if (!program || (!emissionScales.empty() && emissionScales.size() < partMatrices.size()))
    {
        return;
    }
    glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE);
    glUseProgram(program);
    glUniform1f(shotLocation, glow);
    for (std::size_t i = 0; i < textures.size(); ++i)
    {
        glActiveTexture(GL_TEXTURE0 + static_cast<GLenum>(i));
        glBindTexture(GL_TEXTURE_2D, textures[i]);
    }
    for (const auto& mesh : meshes)
    {
        if (mesh.partIndex >= partMatrices.size())
        {
            continue;
        }
        const glm::mat4 model = view * partMatrices[mesh.partIndex];
        glUniform1f(emissionLocation,
                    baseEmission * (emissionScales.empty() ? 1 : emissionScales[mesh.partIndex]));
        const glm::mat4 transform = projection * model;
        glUniformMatrix4fv(modelLocation, 1, GL_FALSE, glm::value_ptr(model));
        glUniformMatrix4fv(transformLocation, 1, GL_FALSE, glm::value_ptr(transform));
        glBindVertexArray(mesh.vao);
        glDrawElements(GL_TRIANGLES, mesh.indices, GL_UNSIGNED_INT, nullptr);
    }
    glBindVertexArray(0);
    glUseProgram(0);
    for (std::size_t i = 0; i < textures.size(); ++i)
    {
        glActiveTexture(GL_TEXTURE0 + static_cast<GLenum>(i));
        glBindTexture(GL_TEXTURE_2D, 0);
    }
    glActiveTexture(GL_TEXTURE0);
}

void AtlasModel::Clear()
{
    for (const auto& mesh : meshes)
    {
        glDeleteVertexArrays(1, &mesh.vao);
        glDeleteBuffers(1, &mesh.vbo);
        glDeleteBuffers(1, &mesh.ebo);
    }
    meshes.clear();
    glDeleteTextures(static_cast<GLsizei>(textures.size()), textures.data());
    textures.fill(0);
    if (program)
    {
        glUseProgram(0);
        glDeleteProgram(program);
        program = 0;
    }
    transformLocation = modelLocation = shotLocation = emissionLocation = -1;
}

} // namespace aegis
