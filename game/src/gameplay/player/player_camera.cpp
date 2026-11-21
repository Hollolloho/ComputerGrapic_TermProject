#include "player_camera.h"

#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>

namespace aegis
{

PlayerCamera::PlayerCamera()
{
    Reset();
}

void PlayerCamera::Reset()
{
    // 시험 복도의 입구 바닥 중심에서 -Z 방향을 바라본다.
    SetPose(glm::vec3(0.0f), 0.0f, 0.0f);
}

void PlayerCamera::SetPose(const glm::vec3& position, float rotYDegrees, float rotXDegrees)
{
    rootPosition = position;
    rotY = rotYDegrees;
    rotX = std::clamp(rotXDegrees, -89.0f, 89.0f);
}

// Pratice24의 model * local eye/target 방식으로 부모 위치와 회전을 적용한다.
glm::mat4 PlayerCamera::ParentTransform() const
{
    glm::mat4 parent = glm::translate(glm::mat4(1.0f), rootPosition);
    return glm::rotate(parent, glm::radians(rotY), glm::vec3(0.0f, 1.0f, 0.0f));
}

glm::vec3 PlayerCamera::EyePosition() const
{
    return glm::vec3(ParentTransform() * glm::vec4(0.0f, eyeHeight, 0.0f, 1.0f));
}

glm::vec3 PlayerCamera::ViewDirection() const
{
    const float angle = glm::radians(rotX);
    const glm::vec4 localDirection(0.0f, std::sin(angle), -std::cos(angle), 0.0f);
    return glm::normalize(glm::vec3(ParentTransform() * localDirection));
}

glm::mat4 PlayerCamera::ViewMatrix() const
{
    const glm::vec3 eye = EyePosition();
    return glm::lookAt(eye, eye + ViewDirection(), glm::vec3(0.0f, 1.0f, 0.0f));
}

} // namespace aegis
