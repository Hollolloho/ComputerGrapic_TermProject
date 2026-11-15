#include "player_camera.h"

#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>

namespace aegis {

PlayerCamera::PlayerCamera()
{
    Reset();
}

void PlayerCamera::Reset()
{
    // 기존 시선 방향은 유지하고 거리를 1.5배 늘려 드론을 멀리서 본다.
    const glm::vec3 target(0.13f, -0.025f, 0.02f);
    const glm::vec3 eye = target + (glm::vec3(1.5f, 1.85f, 3.4f) - target) * 1.5f;
    const glm::vec3 direction = glm::normalize(target - eye);
    SetPose(eye - glm::vec3(0.0f, eyeHeight, 0.0f),
            glm::degrees(std::atan2(-direction.x, -direction.z)),
            glm::degrees(std::asin(direction.y)));
}

void PlayerCamera::SetPose(const glm::vec3& position, float yawDegrees, float pitchDegrees)
{
    rootPosition = position;
    yaw = yawDegrees;
    pitch = std::clamp(pitchDegrees, -89.0f, 89.0f);
}

// Pratice24의 model * local eye/target 방식으로 부모 위치와 회전을 적용한다.
glm::mat4 PlayerCamera::ParentTransform() const
{
    glm::mat4 parent = glm::translate(glm::mat4(1.0f), rootPosition);
    return glm::rotate(parent, glm::radians(yaw), glm::vec3(0.0f, 1.0f, 0.0f));
}

glm::vec3 PlayerCamera::EyePosition() const
{
    return glm::vec3(ParentTransform() * glm::vec4(0.0f, eyeHeight, 0.0f, 1.0f));
}

glm::vec3 PlayerCamera::ViewDirection() const
{
    const float angle = glm::radians(pitch);
    const glm::vec4 localDirection(0.0f, std::sin(angle), -std::cos(angle), 0.0f);
    return glm::normalize(glm::vec3(ParentTransform() * localDirection));
}

glm::mat4 PlayerCamera::ViewMatrix() const
{
    const glm::vec3 eye = EyePosition();
    return glm::lookAt(eye, eye + ViewDirection(), glm::vec3(0.0f, 1.0f, 0.0f));
}

} // namespace aegis
