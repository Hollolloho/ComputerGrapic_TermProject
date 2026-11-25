// 캠페인의 입구와 전투 환경을 불러오고 그린다.
#include "campaign_environment.h"
#include "campaign_layout.generated.h"
#include "gameplay/player/player_settings.h"
#include "combat_map/terminal_layout.generated.h"
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>
#include <fstream>
#include <iostream>

namespace aegis
{
namespace
{

using corridor_asset::placements;

const glm::vec3 Up(0.0f, 1.0f, 0.0f);

} // namespace

bool CampaignEnvironment::Init(const std::filesystem::path& root)
{
    Clear();
    glEnable(GL_DEPTH_TEST);
    // 메시 셰이더가 최종 색을 sRGB로 변환한다.
    glDisable(GL_FRAMEBUFFER_SRGB);
    glClearColor(0.16f, 0.19f, 0.24f, 1.0f);
    return renderer.Init(root / "shaders") && InitCorridor(root) && InitEntry(root) &&
           combatMap.Init(root);
}

void CampaignEnvironment::SetEntryCamera()
{
    camera.SetPose(campaign_asset::playerStart, campaign_asset::playerRotYDegrees, campaign_asset::playerRotXDegrees);
}

bool CampaignEnvironment::InitCorridor(const std::filesystem::path& projectDirectory)
{
    const auto directory = projectDirectory / "asset" / "corridor" / "test_scene";
    const auto materialPath = directory / "materials.mtl";
    if (!std::ifstream(materialPath))
    {
        std::cerr << "Could not read corridor materials: " << materialPath << '\n';
        return false;
    }
    const ObjModelCache cache(projectDirectory / "build" / "cache" / "models", materialPath);

    for (std::size_t i = 0; i < corridor.size(); ++i)
    {
        auto& object = corridor[i];
        const auto& placement = placements[i];
        const glm::mat4 parent = placement.parent >= 0 ? corridor[placement.parent].model : glm::mat4(1.0f);
        object.model = glm::rotate(glm::translate(parent, placement.translation),
                                   glm::radians(placement.rotYDegrees), Up);

        const auto path = directory / placement.file;
        ModelData model;
        std::string error;
        bool cacheHit{};
        if (!cache.Load(path, model, error, cacheHit))
        {
            std::cerr << error << '\n';
            return false;
        }

        for (const auto& mesh : model.meshes)
        {
            const auto& material = model.materials[mesh.materialIndex];
            auto& drawMesh = object.meshes.emplace_back();
            if (!drawMesh.Upload(mesh, material))
                return false;
            // MTL의 반사색과 Phong 지수로 비금속 GGX 재질을 근사한다.
            drawMesh.dielectricF0 = glm::clamp(material.specular, 0.0f, 1.0f);
            drawMesh.roughness = std::sqrt(2.0f / (glm::max(material.shininess, 0.0f) + 2.0f));
        }
    }
    return true;
}

bool CampaignEnvironment::InitEntry(const std::filesystem::path& root)
{
    return LoadDrawMeshes(root / campaign_asset::entryMesh, entryRoom) &&
           LoadDrawMeshes(root / terminal_asset::mesh, entryTerminal);
}

void CampaignEnvironment::DrawEntry(int width, int height) const
{
    glViewport(0, 0, width, height);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_FRAMEBUFFER_SRGB);
    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glDisable(GL_CULL_FACE);
    glClearColor(0.03f, 0.06f, 0.09f, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    const auto viewProjection =
        glm::perspective(glm::radians(player::WorldFovY), static_cast<float>(width) / height, 0.05f, 80.0f) *
        camera.ViewMatrix();
    renderer.Begin(camera.EyePosition(), {0.45f, 0.9f, 0.7f}, {1, 1, 1}, 3, 0.35f);
    renderer.Draw(entryRoom, viewProjection, glm::mat4(1));
    for (const auto& object : corridor)
    {
        renderer.Draw(object.meshes, viewProjection, object.model);
    }
    renderer.Draw(entryTerminal, viewProjection,
                  glm::rotate(glm::translate(glm::mat4(1), campaign_asset::terminalPosition),
                              glm::radians(campaign_asset::terminalRotYDegrees), Up));
}

void CampaignEnvironment::DrawBattle(const glm::mat4& viewProjection, const glm::vec3& eye,
                                     bool useCombatMap, float doorProgress) const
{
    renderer.Begin(eye, {0.45f, 0.9f, 0.7f}, {1, 1, 1}, 2.5f, 0.45f);
    if (useCombatMap)
    {
        combatMap.Draw(viewProjection, eye);
        return;
    }
    for (std::size_t i = 0; i < corridor.size(); ++i)
    {
        auto model = corridor[i].model;
        if (i == 2 || i == 3)
        {
            const float offset = (i == 2 ? -1 : 1) * corridor_asset::doorTravel * doorProgress;
            model = glm::translate(model, glm::vec3(offset, 0, 0));
        }
        renderer.Draw(corridor[i].meshes, viewProjection, model);
    }
}

void CampaignEnvironment::Clear()
{
    for (auto& object : corridor)
    {
        ClearDrawMeshes(object.meshes);
    }
    combatMap.Clear();
    ClearDrawMeshes(entryRoom);
    ClearDrawMeshes(entryTerminal);
    renderer.Clear();
}

} // namespace aegis
