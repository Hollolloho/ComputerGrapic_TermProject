#pragma once

#include "gameplay/combat/combat.h"
#include "loader/RenderMesh.h"
#include <glm/mat4x4.hpp>

namespace aegis::combat { class CombatEncounter; struct CrystalShard; }

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
    void DrawTrails(const glm::mat4& projection, const glm::mat4& view, const glm::vec3& eye,
                    const combat::CombatEncounter& encounter) const;
    void Clear();

private:
    void DrawModel(const std::vector<RenderMesh>& meshes, const glm::mat4& projection,
                   const glm::mat4& modelView, const glm::vec3& color) const;
    void DrawTrail(const glm::mat4& projection, const glm::mat4& view, const glm::vec3& eye,
                   const combat::CrystalTrail& trail) const;
    void DrawEffect(const glm::mat4& projection, const glm::mat4& modelView, float fadeStart, float fadeEnd) const;
    std::vector<RenderMesh> body, chip;
    MeshBuffer effectQuad;
    GLuint program{};
    GLint transformLocation{-1}, modelViewLocation{-1}, colorLocation{-1};
    GLint effectLocation{-1}, effectFadeLocation{-1};
};
}
