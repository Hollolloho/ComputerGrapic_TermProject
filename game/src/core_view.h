#pragma once

#include "mesh_renderer.h"
#include <array>
#include <glm/vec3.hpp>

namespace aegis
{
class CoreView
{
public:
    bool Init(const std::filesystem::path& root);
    // 자력 단계는 0=꺼짐, 1=경고, 2=작동이다.
    void Draw(const glm::mat4& viewProjection, const glm::vec3& eye, bool coreAlive,
              int magneticStage, float phaseProgress, float fieldRadius) const;
    void Clear();

private:
    struct Triangle
    {
        glm::vec3 center;
        std::array<unsigned int, 3> indices;
    };
    struct CoreMesh
    {
        DrawMesh mesh;
        glm::vec3 emission{};
        glm::vec3 center{};
        float opacity{1};
        std::vector<Triangle> triangles;
    };

    bool LoadPart(const std::filesystem::path& path, std::vector<CoreMesh>& meshes, bool transparent);
    void DrawPart(const std::vector<CoreMesh>& meshes, const glm::mat4& model, int layer,
                  float power) const;

    std::vector<CoreMesh> structure, hologram, glass;
    DrawMesh ring;
    GLuint program{};
    GLint transformLocation{-1}, modelLocation{-1}, eyeLocation{-1}, colorLocation{-1};
    GLint emissionLocation{-1}, metallicLocation{-1}, roughnessLocation{-1}, opacityLocation{-1};
    GLint layerLocation{-1}, powerLocation{-1};
};
} // namespace aegis
