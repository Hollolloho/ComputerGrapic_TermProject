#pragma once

#include "fixed_turret.h"
#include "gameplay/rendering/mesh_renderer.h"
#include <array>

namespace aegis
{
class Turret
{
public:
    bool Init(const std::filesystem::path& root, combat::TurretType type);
    void Draw(const glm::mat4& viewProjection, const glm::vec3& eye,
              const combat::FixedTurret& turret, float debrisAge = -1.f) const;
    void Clear();

private:
    struct TurretMesh
    {
        DrawMesh mesh;
        int group{};
        int piece{};
        bool weapon{};
    };
    std::vector<TurretMesh> meshes;
    std::array<GLuint, 4> textures{};
    GLuint program{};
    GLint transformLocation{-1}, modelLocation{-1}, eyeLocation{-1}, warningLocation{-1}, colorLocation{-1};
};
} // namespace aegis
