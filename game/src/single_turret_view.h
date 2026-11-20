#pragma once

#include "fixed_turret.h"
#include "mesh_renderer.h"
#include <array>

namespace aegis
{
class SingleTurretView
{
public:
    bool Init(const std::filesystem::path& root);
    void Draw(const glm::mat4& viewProjection, const glm::vec3& eye,
              const combat::FixedTurret& turret, float spawnYawDegrees) const;
    void Clear();

private:
    struct TurretMesh
    {
        DrawMesh mesh;
        int group{};
        bool weapon{};
    };
    std::vector<TurretMesh> meshes;
    std::array<GLuint, 4> textures{};
    GLuint program{};
    GLint transformLocation{-1}, modelLocation{-1}, eyeLocation{-1}, warningLocation{-1};
};
} // namespace aegis
