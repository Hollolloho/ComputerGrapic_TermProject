#pragma once
#include "gameplay/rendering/atlas_model.h"
#include "gameplay/boss/boss_encounter.h"
#include "gameplay/rendering/mesh_renderer.h"

namespace aegis
{
class BossView
{
public:
    bool Init(const std::filesystem::path& root);
    void Draw(const glm::mat4& projection, const glm::mat4& view, const glm::vec3& eye,
              const combat::BossEncounter& boss) const;
    void Clear();

private:
    AtlasModel model;
    MeshRenderer renderer;
    std::vector<DrawMesh> room, cover, supply;
};
} // namespace aegis
