#include "game_scene.h"
#include "corridor_layout.generated.h"

#include <GL/glew.h>
#include <glm/gtc/matrix_transform.hpp>

#include <cmath>
#include <fstream>
#include <iostream>

namespace aegis {
namespace {

using corridor_asset::placements;

const glm::vec3 Up(0.0f, 1.0f, 0.0f);

} // namespace

bool GameScene::Init(const std::filesystem::path& projectDirectory, bool useCombatTest, bool turretTest)
{
    Clear();
    legacyTest=useCombatTest;
    glEnable(GL_DEPTH_TEST);
    // 메시 셰이더가 최종 색을 sRGB로 변환한다.
    glDisable(GL_FRAMEBUFFER_SRGB);
    glClearColor(0.16f, 0.19f, 0.24f, 1.0f);
    if (!renderer.Init(projectDirectory / "game" / "shaders") || !InitCorridor(projectDirectory)
        || !testScene.Init(projectDirectory)) return false;
    testScene.SetCorridorMode(!legacyTest);
    if (legacyTest) testScene.SetTurretMode(turretTest);
    return true;
}

bool GameScene::InitCorridor(const std::filesystem::path& projectDirectory)
{
    const auto directory = projectDirectory / "asset" / "corridor" / "test_scene";
    const auto materialPath = directory / "materials.mtl";
    if (!std::ifstream(materialPath)) {
        std::cerr << "Could not read corridor materials: " << materialPath << '\n';
        return false;
    }
    const ObjModelCache cache(projectDirectory / "build" / "cache" / "models", materialPath);

    static_assert(std::size(placements) == std::tuple_size_v<decltype(corridor)>);
    for (std::size_t i = 0; i < corridor.size(); ++i) {
        auto& object = corridor[i];
        const auto& placement = placements[i];
        const glm::mat4 parent = placement.parent>=0 ? corridor[placement.parent].model : glm::mat4(1.0f);
        object.model = glm::rotate(glm::translate(parent, placement.translation),
                                   glm::radians(placement.yawDegrees), Up);

        const auto path = directory / placement.file;
        ModelData model;
        std::string error;
        bool cacheHit{};
        if (!cache.Load(path, model, error, cacheHit)) {
            std::cerr << error << '\n';
            return false;
        }

        for (const auto& mesh : model.meshes) {
            const auto& material = model.materials[mesh.materialIndex];
            auto& drawMesh = object.meshes.emplace_back();
            drawMesh.Upload(mesh, material);
            // MTL의 반사색과 Phong 지수로 비금속 GGX 재질을 근사한다.
            drawMesh.dielectricF0 = glm::clamp(material.specular, 0.0f, 1.0f);
            drawMesh.roughness = std::sqrt(2.0f / (glm::max(material.shininess, 0.0f) + 2.0f));
        }
    }
    return true;
}

void GameScene::Update(double deltaTime)
{
    testScene.Update(deltaTime,deltaTime);
}

void GameScene::Draw(int width, int height) const
{
    if (legacyTest) { testScene.Draw(width,height); return; }
    testScene.Draw(width,height,[this](const glm::mat4& vp,const glm::vec3& eye) {
        renderer.Begin(eye,{0.45f,0.9f,0.7f},{1,1,1},2.5f,0.45f);
        for (const auto& object:corridor) renderer.Draw(object.meshes,vp,object.model);
    });
}

void GameScene::Reset()
{
    drone.Reset();
    camera.Reset();
    testScene.Reset();
}

void GameScene::Clear()
{
    drone.Clear();
    testScene.Clear();
    for (auto& object : corridor) {
        for (auto& mesh : object.meshes) {
            mesh.Clear();
        }
        object.meshes.clear();
    }
    renderer.Clear();
}

void GameScene::ToggleMode() {
    legacyTest=!legacyTest;
    testScene.SetCorridorMode(!legacyTest);
    if (legacyTest) testScene.SetTurretMode(false);
}
void GameScene::SampleInput(GLFWwindow* window,bool active) { testScene.SampleInput(window,active); }

} // namespace aegis
