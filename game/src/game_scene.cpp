#include "game_scene.h"

#include <GL/glew.h>
#include <glm/gtc/matrix_transform.hpp>

#include <cmath>
#include <fstream>
#include <iostream>

namespace aegis {
namespace {

// asset/corridor/test_scene/scene_manifest.json의 닫힌 문 기준 배치다.
enum CorridorIndex { Shell, DoorFrame, LeftDoor, RightDoor, Cover };

struct CorridorPlacement {
    const char* file;
    glm::vec3 translation;
    float yawDegrees;
};

const CorridorPlacement CorridorPlacements[] = {
    {"corridor_static.obj", { 0.00f, 0.0f,   0.00f},  0.0f},
    {"door_frame.obj",      { 0.00f, 0.0f, -17.81f},  0.0f},
    {"door_left.obj",       {-0.72f, 0.0f,   0.00f},  0.0f},
    {"door_right.obj",      { 0.72f, 0.0f,   0.00f},  0.0f},
    {"cover_cabinet.obj",   {-1.20f, 0.0f,  -8.40f}, 90.0f},
};

const glm::vec3 Up(0.0f, 1.0f, 0.0f);

} // namespace

bool GameScene::Init(const std::filesystem::path& projectDirectory)
{
    Clear();
    glEnable(GL_DEPTH_TEST);
    // 메시 셰이더가 최종 색을 sRGB로 변환한다.
    glDisable(GL_FRAMEBUFFER_SRGB);
    glClearColor(0.16f, 0.19f, 0.24f, 1.0f);
    return renderer.Init(projectDirectory / "game" / "shaders") &&
           InitCorridor(projectDirectory) && drone.Init(projectDirectory);
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

    static_assert(std::size(CorridorPlacements) == std::tuple_size_v<decltype(corridor)>);
    for (std::size_t i = 0; i < corridor.size(); ++i) {
        auto& object = corridor[i];
        const auto& placement = CorridorPlacements[i];
        const bool isDoor = i == LeftDoor || i == RightDoor;
        const glm::mat4 parent = isDoor ? corridor[DoorFrame].model : glm::mat4(1.0f);
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
    drone.Update(deltaTime);
}

void GameScene::Draw(int width, int height) const
{
    glViewport(0, 0, width, height);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    const float aspect = static_cast<float>(width) / static_cast<float>(height);
    const glm::mat4 projection = glm::perspective(glm::radians(55.0f), aspect, 0.05f, 30.0f);
    const glm::vec3 camPos = camera.EyePosition();
    const glm::mat4 scene = projection * camera.ViewMatrix();
    const glm::vec3 lightDirection(0.45f, 0.9f, 0.7f);
    const glm::vec3 lightColor(1.0f);
    renderer.Begin(camPos, lightDirection, lightColor, 2.5f, 0.45f);
    for (const auto& object : corridor) {
        renderer.Draw(object.meshes, scene, object.model);
    }

    renderer.Begin(camPos, lightDirection, lightColor, 5.0f, 0.16f);
    const glm::mat4 droneWorld = glm::translate(glm::mat4(1.0f), glm::vec3(1.65f, 1.65f, -6.2f));
    drone.Draw(renderer, scene, droneWorld);
}

void GameScene::Reset()
{
    drone.Reset();
    camera.Reset();
}

void GameScene::Clear()
{
    drone.Clear();
    for (auto& object : corridor) {
        for (auto& mesh : object.meshes) {
            mesh.Clear();
        }
        object.meshes.clear();
    }
    renderer.Clear();
}

} // namespace aegis
