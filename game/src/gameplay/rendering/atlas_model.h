#pragma once
#include "loader/RenderMesh.h"
#include <glm/mat4x4.hpp>
#include <array>
#include <filesystem>
#include <span>
#include <vector>

namespace aegis
{
struct AtlasPart
{
    const char* mesh;
    int triangles;
};

struct AtlasTexture
{
    const char* name;
    const char* file;
    bool srgb;
};

// Shared five-map PBR resources; gameplay and rig transforms remain external.
class AtlasModel
{
public:
    AtlasModel() = default;
    AtlasModel(const AtlasModel&) = delete;
    AtlasModel& operator=(const AtlasModel&) = delete;
    bool Init(const std::filesystem::path& root, const std::filesystem::path& runtime,
              const std::vector<AtlasPart>& parts, const std::array<AtlasTexture, 5>& textureSpecs,
              float emission);
    // span은 보스의 14부품과 무기의 8부품 배열을 복사 없이 읽는다. 배열의 길이도 함께 전달된다.
    void Draw(const glm::mat4& projection, const glm::mat4& view, std::span<const glm::mat4> partMatrices,
              float glow = 0, std::span<const float> emissionScales = {}) const;
    void Clear();

private:
    struct Mesh : MeshBuffer
    {
        std::size_t partIndex{};
    };

    std::vector<Mesh> meshes;
    std::array<GLuint, 5> textures{};
    GLuint program{};
    GLint transformLocation{-1};
    GLint modelLocation{-1};
    GLint shotLocation{-1};
    GLint emissionLocation{-1};
    float baseEmission{1};
};
} // namespace aegis
