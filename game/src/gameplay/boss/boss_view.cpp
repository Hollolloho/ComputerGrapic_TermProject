#include "boss_view.h"
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>

namespace aegis
{
bool BossView::Init(const std::filesystem::path& root)
{
    Clear();
    std::vector<AtlasPart> parts(boss_asset::partCount);
    std::array<AtlasTexture, 5> textures;
    for (std::size_t i = 0; i < parts.size(); ++i)
    {
        parts[i] = {boss_asset::parts[i].mesh, boss_asset::parts[i].triangles};
    }
    for (std::size_t i = 0; i < textures.size(); ++i)
    {
        const auto& t = boss_asset::textures[i];
        textures[i] = {t.name, t.file, t.srgb};
    }
    return model.Init(root, root / "asset/boss_e/runtime", parts, textures, boss_asset::emission) &&
           renderer.Init(root / "shaders") &&
           LoadDrawMeshes(root / "asset/core_room/runtime/obj/core_room.obj", room) &&
           LoadDrawMeshes(root / "asset/corridor/test_scene/cover_cabinet.obj", cover) &&
           LoadDrawMeshes(root / "asset/props/runtime/obj/cw01c_supply_station.obj", supply);
}

void BossView::Draw(const glm::mat4& projection, const glm::mat4& view, const glm::vec3& eye,
                    const combat::BossEncounter& boss) const
{
    const auto viewProjection = projection * view;
    renderer.Begin(eye, {0.4f, 0.9f, 0.7f}, {1, 1, 1}, 3, 0.35f);
    renderer.Draw(room, viewProjection, glm::mat4(1));
    for (const auto& p : boss_asset::covers)
    {
        const auto matrix = glm::scale(
            glm::rotate(glm::translate(glm::mat4(1), p.translation), glm::radians(p.rotYDegrees), {0, 1, 0}),
            p.scale);
        renderer.Draw(cover, viewProjection, matrix);
    }
    renderer.Draw(supply, viewProjection, glm::translate(glm::mat4(1), boss_asset::supplyPosition));
    std::array<float, boss_asset::partCount> glow;
    glow.fill(1);
    const bool warning =
        boss.Phase() == combat::BossPhase::Warning || boss.Phase() == combat::BossPhase::Attack;
    for (const int i : {boss_asset::part_sector_warning, boss_asset::part_turret_warning_L,
                        boss_asset::part_turret_warning_R})
    {
        glow[i] = warning ? 3.0f : 0.25f;
    }
    glow[boss_asset::part_core_crystal] = boss.CoreHits() >= 3                         ? 0
                                          : boss.Phase() == combat::BossPhase::Exposed ? 3.0f
                                                                                       : 1.0f;
    model.Draw(projection, view, boss.Pose().parts, 0, glow);
}

void BossView::Clear()
{
    model.Clear();
    renderer.Clear();
    ClearDrawMeshes(room);
    ClearDrawMeshes(cover);
    ClearDrawMeshes(supply);
}
} // namespace aegis
