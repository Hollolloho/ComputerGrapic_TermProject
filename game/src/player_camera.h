#pragma once

#include <glm/glm.hpp>

namespace aegis {

class PlayerCamera {
public:
    PlayerCamera();
    void Reset();
    void SetPose(const glm::vec3& position, float yawDegrees, float pitchDegrees);
    glm::mat4 ParentTransform() const;
    glm::vec3 EyePosition() const;
    glm::vec3 ViewDirection() const;
    glm::mat4 ViewMatrix() const;

private:
    glm::vec3 rootPosition{};
    float yaw{};
    float pitch{};
    const float eyeHeight = 1.65f;
};

} // namespace aegis
