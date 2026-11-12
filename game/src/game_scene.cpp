#include "game_scene.h"

#include <GL/glew.h>
#include <glm/gtc/matrix_transform.hpp>

namespace aegis {

bool GameScene::Init(const std::filesystem::path& projectDirectory)
{
    glEnable(GL_DEPTH_TEST);
    // 드론 셰이더가 최종 색을 sRGB로 변환한다.
    glDisable(GL_FRAMEBUFFER_SRGB);
    glClearColor(0.16f, 0.19f, 0.24f, 1.0f);
    return drone.Init(projectDirectory);
}

void GameScene::Update(double deltaTime)
{
    drone.Update(deltaTime);
}

void GameScene::Draw(int width, int height) const
{
    glViewport(0, 0, width, height);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // 프레임버퍼 비율에 맞춰 투영을 계산하고 기존 고정 카메라 시점을 유지한다.
    const float aspect = static_cast<float>(width) / static_cast<float>(height);
    const glm::mat4 projection = glm::perspective(glm::radians(33.0f), aspect, 0.1f, 20.0f);
    const glm::vec3 camPos(1.5f, 1.85f, 3.4f);
    const glm::vec3 camTarget(0.13f, -0.025f, 0.02f);
    const glm::vec3 camUp(0.0f, 1.0f, 0.0f);
    const glm::mat4 scene = projection * glm::lookAt(camPos, camTarget, camUp);
    const glm::vec3 lightDirection(0.45f, 0.9f, 0.7f);
    const glm::vec3 lightColor(1.0f);
    const float lightStrength = 5.0f;
    drone.Draw(scene, camPos, lightDirection, lightColor, lightStrength);
}

void GameScene::Reset()
{
    drone.Reset();
}

void GameScene::Clear()
{
    drone.Clear();
}

} // namespace aegis
