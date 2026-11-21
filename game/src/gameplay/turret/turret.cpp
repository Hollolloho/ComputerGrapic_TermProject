#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <wincodec.h>
#include <wrl/client.h>

#include "turret.h"
#include "loader/Shader.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <iostream>

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

GLuint LoadPng(IWICImagingFactory* factory, const std::filesystem::path& path, bool srgb)
{
    ComPtr<IWICBitmapDecoder> decoder;
    ComPtr<IWICBitmapFrameDecode> frame;
    ComPtr<IWICFormatConverter> converter;
    HRESULT result = factory->CreateDecoderFromFilename(path.c_str(), nullptr, GENERIC_READ,
                                                        WICDecodeMetadataCacheOnLoad, &decoder);
    if (SUCCEEDED(result)) result = decoder->GetFrame(0, &frame);
    if (SUCCEEDED(result)) result = factory->CreateFormatConverter(&converter);
    if (SUCCEEDED(result))
    {
        result = converter->Initialize(frame.Get(), GUID_WICPixelFormat32bppRGBA,
                                      WICBitmapDitherTypeNone, nullptr, 0, WICBitmapPaletteTypeCustom);
    }
    UINT width{}, height{};
    if (SUCCEEDED(result)) result = converter->GetSize(&width, &height);
    if (FAILED(result) || width == 0 || height == 0 || width > 8192 || height > 8192)
    {
        std::cerr << "Cannot decode turret texture: " << path << '\n';
        return 0;
    }
    const UINT stride = width * 4;
    std::vector<unsigned char> pixels(static_cast<std::size_t>(stride) * height);
    result = converter->CopyPixels(nullptr, stride, static_cast<UINT>(pixels.size()), pixels.data());
    if (FAILED(result))
    {
        std::cerr << "Cannot read turret texture pixels: " << path << '\n';
        return 0;
    }
    // OBJ의 UV 원점에 맞춰 행 순서만 바꾸고 OpenGL 노멀 맵의 녹색 값은 유지한다.
    for (UINT y = 0; y < height / 2; ++y)
    {
        std::swap_ranges(pixels.begin() + static_cast<std::size_t>(y) * stride,
                         pixels.begin() + static_cast<std::size_t>(y + 1) * stride,
                         pixels.begin() + static_cast<std::size_t>(height - y - 1) * stride);
    }
    GLuint texture{};
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, srgb ? GL_SRGB8_ALPHA8 : GL_RGBA8,
                 static_cast<GLsizei>(width), static_cast<GLsizei>(height), 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    GLint uploadedWidth{};
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &uploadedWidth);
    if (uploadedWidth != static_cast<GLint>(width))
    {
        std::cerr << "Cannot upload turret texture: " << path << '\n';
        glDeleteTextures(1, &texture);
        return 0;
    }
    return texture;
}
} // namespace

bool Turret::Init(const std::filesystem::path& root, combat::TurretType type)
{
    Clear();
    program = LoadShaderProgram(root / "shaders/turret_model.vert", root / "shaders/turret_model.frag");
    if (!program)
    {
        return false;
    }
    transformLocation = glGetUniformLocation(program, "uTransform");
    modelLocation = glGetUniformLocation(program, "uModel");
    eyeLocation = glGetUniformLocation(program, "uEye");
    warningLocation = glGetUniformLocation(program, "uWarning");
    colorLocation = glGetUniformLocation(program, "uMaterialColor");
    if (colorLocation < 0 || transformLocation < 0 || modelLocation < 0 || eyeLocation < 0 || warningLocation < 0)
    {
        std::cerr << "Turret shader uniforms are missing\n";
        Clear();
        return false;
    }
    const std::string modelName = type == combat::TurretType::Twin ? "Twin" : "Single_C";
    const auto directory = root / "asset/AEGIS_Turrets" / modelName;
    ModelData data;
    std::string error;
    if (!LoadModel(directory / (modelName + ".obj"), data, error))
    {
        std::cerr << error << '\n';
        Clear();
        return false;
    }
    for (const auto& source : data.meshes)
    {
        if (type == combat::TurretType::SingleC && !source.hasUvs)
        {
            std::cerr << "Single_C mesh has no UV coordinates: " << source.name << '\n';
            Clear();
            return false;
        }
        auto& draw = meshes.emplace_back();
        draw.mesh.Upload(source, data.materials.at(source.materialIndex));
        const auto& name = source.name;
        draw.weapon = name.find("Barrel") != std::string::npos ||
                      name.find("Bore") != std::string::npos || name.find("Mantlet") != std::string::npos;
        const bool pitch = draw.weapon || name.find("Exposed_core") != std::string::npos;
        const bool base = name.find("Foundation") != std::string::npos ||
                          name.find("Lower_core") != std::string::npos ||
                          name.find("Anchor") != std::string::npos || name.find("Slope") != std::string::npos ||
                          name.find("Upper_base") != std::string::npos || name.find("Bearing") != std::string::npos;
        draw.group = base ? 0 : (pitch ? 2 : 1);
        glBindVertexArray(draw.mesh.vao);
        glBindBuffer(GL_ARRAY_BUFFER, draw.mesh.vbo);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(ModelVertex),
                              reinterpret_cast<void*>(offsetof(ModelVertex, uv)));
        glEnableVertexAttribArray(2);
    }
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glUseProgram(program);
    glUniform1i(glGetUniformLocation(program, "uTextured"), type == combat::TurretType::SingleC);
    glUseProgram(0);
    if (type == combat::TurretType::Twin)
    {
        return !meshes.empty();
    }
    ComContext com;
    ComPtr<IWICImagingFactory> factory;
    if ((FAILED(com.status) && com.status != RPC_E_CHANGED_MODE) ||
        FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                                IID_PPV_ARGS(&factory))))
    {
        std::cerr << "Cannot initialize Single_C texture decoder\n";
        Clear();
        return false;
    }
    constexpr std::array<const char*, 4> files{"BaseColor.png", "Normal.png", "Metallic.png", "Roughness.png"};
    constexpr std::array<const char*, 4> samplers{"uBaseColorMap", "uNormalMap", "uMetallicMap", "uRoughnessMap"};
    glUseProgram(program);
    for (std::size_t i = 0; i < textures.size(); ++i)
    {
        textures[i] = LoadPng(factory.Get(), directory / files[i], i == 0);
        const GLint sampler = glGetUniformLocation(program, samplers[i]);
        if (!textures[i] || sampler < 0)
        {
            std::cerr << "Cannot initialize Single_C map: " << files[i] << '\n';
            Clear();
            return false;
        }
        glUniform1i(sampler, static_cast<GLint>(i));
    }
    glBindTexture(GL_TEXTURE_2D, 0);
    glUseProgram(0);
    std::cout << "Single_C: meshes=" << meshes.size() << " textures=" << textures.size() << '\n';
    return !meshes.empty();
}

void Turret::Draw(const glm::mat4& viewProjection, const glm::vec3& eye,
                  const combat::FixedTurret& turret) const
{
    if (!program || !turret.Alive())
    {
        return;
    }
    // 배포 OBJ는 Z 상향, +Y 정면이다. (-x, z, y)로 회전해 게임 축에 맞춘다.
    const auto turn = glm::rotate(glm::mat4(1), glm::radians(180.0f), glm::vec3(0, 1, 0));
    const auto axes = glm::rotate(turn, glm::radians(-90.0f), glm::vec3(1, 0, 0));
    const auto position = glm::translate(glm::mat4(1), turret.Position());
    const auto base = position;
    const auto barrel = turret.BarrelTransform();
    const auto forward = glm::vec3(barrel[2]);
    const float yaw = std::atan2(forward.x, forward.z);
    const auto head = glm::rotate(position, yaw, glm::vec3(0, 1, 0));
    const std::array<glm::mat4, 3> transforms{
        base * axes, head * axes, glm::translate(barrel, glm::vec3(0, -1.62f, 0)) * axes};

    glEnable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE);
    glUseProgram(program);
    glUniform3fv(eyeLocation, 1, glm::value_ptr(eye));
    glUniform1f(warningLocation, turret.Phase() == combat::TurretPhase::Warning ? 1.0f : 0.0f);
    for (std::size_t i = 0; i < textures.size(); ++i)
    {
        glActiveTexture(GL_TEXTURE0 + static_cast<GLenum>(i));
        glBindTexture(GL_TEXTURE_2D, textures[i]);
    }
    for (const auto& draw : meshes)
    {
        if (draw.weapon && !turret.Armed())
        {
            continue;
        }
        glUniform3fv(colorLocation, 1, glm::value_ptr(draw.mesh.baseColor));
        const auto& model = transforms[draw.group];
        const auto transform = viewProjection * model;
        glUniformMatrix4fv(modelLocation, 1, GL_FALSE, glm::value_ptr(model));
        glUniformMatrix4fv(transformLocation, 1, GL_FALSE, glm::value_ptr(transform));
        glBindVertexArray(draw.mesh.vao);
        glDrawElements(GL_TRIANGLES, draw.mesh.indexCount, GL_UNSIGNED_INT, nullptr);
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

void Turret::Clear()
{
    for (auto& draw : meshes)
    {
        draw.mesh.Clear();
    }
    meshes.clear();
    glDeleteTextures(static_cast<GLsizei>(textures.size()), textures.data());
    textures.fill(0);
    if (program)
    {
        glDeleteProgram(program);
        program = 0;
    }
}
} // namespace aegis
