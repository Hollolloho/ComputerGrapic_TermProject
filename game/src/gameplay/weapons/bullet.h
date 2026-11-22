#pragma once

#include "gameplay/combat/combat.h"
#include "loader/RenderMesh.h"
#include <glm/mat4x4.hpp>

namespace aegis::combat { struct CrystalShard; }

namespace aegis
{
class BulletView
{
public:
    bool Init(const std::filesystem::path& root);
    void Draw(const glm::mat4& projection, const glm::mat4& view, const combat::Bullet& bullet,
              const glm::vec3& color, float radius) const;
    void DrawShard(const glm::mat4& projection, const glm::mat4& view, const combat::CrystalShard& shard,
                   const glm::vec3& color) const;
    void Clear();

private:
    void DrawModel(const std::vector<RenderMesh>& meshes, const glm::mat4& projection,
                   const glm::mat4& modelView, const glm::vec3& color) const;
    std::vector<RenderMesh> body, chip;
    GLuint program{};
    GLint transformLocation{-1}, modelViewLocation{-1}, colorLocation{-1};
};
}
