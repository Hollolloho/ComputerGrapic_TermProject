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

    // 투영은 유지하고 부모에 연결된 눈 카메라의 월드 시점을 사용한다.
    const float aspect = static_cast<float>(width) / static_cast<float>(height);
    const glm::mat4 projection = glm::perspective(glm::radians(33.0f), aspect, 0.1f, 20.0f);
    const glm::vec3 camPos = camera.EyePosition();
    const glm::mat4 scene = projection * camera.ViewMatrix();
    const glm::vec3 lightDirection(0.45f, 0.9f, 0.7f);
    const glm::vec3 lightColor(1.0f);
    const float lightStrength = 5.0f;
    drone.Draw(scene, camPos, lightDirection, lightColor, lightStrength);
}

void GameScene::Reset()
{
    drone.Reset();
    camera.Reset();
}

void GameScene::Clear()
{
    drone.Clear();
}

} // namespace aegis
