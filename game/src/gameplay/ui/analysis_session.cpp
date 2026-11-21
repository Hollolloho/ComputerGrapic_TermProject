// 분석 단말의 거리·조준·안전 조건을 확인하고 분석 진입과 종료를 관리한다.
#include "analysis_session.h"
#include <algorithm>
#include <cmath>
#include <glm/geometric.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace aegis::combat
{
float AnalysisSession::TargetDistance(const Collider& terminal, const glm::vec3& eye,
                                      const glm::vec3& direction,
                                      const std::vector<Collider>& obstacles) const
{
    const float length2 = glm::dot(direction, direction);
    if (!std::isfinite(length2) || length2 <= 1e-8f)
    {
        return -1.0f;
    }
    const auto end = eye + glm::normalize(direction) * 2.5f;
    float targetFraction;
    if (!SegmentHitsBox(eye, end, terminal, targetFraction))
    {
        return -1.0f;
    }
    Hit obstruction;
    if (FindFirstHit(eye, end, obstacles, obstruction) && obstruction.objectId != terminal.id &&
        obstruction.fraction <= targetFraction)
    {
        return -1.0f;
    }
    return targetFraction * 2.5f;
}

bool AnalysisSession::IsFirstTarget(const Collider& terminal, const glm::vec3& eye,
                                    const glm::vec3& direction, const std::vector<Collider>& obstacles,
                                    float weaponDistance) const
{
    const float targetDistance = TargetDistance(terminal, eye, direction, obstacles);
    return targetDistance >= 0 && (weaponDistance < 0 || targetDistance <= weaponDistance);
}

bool AnalysisSession::Open(bool safe, const Collider& terminal, const glm::vec3& eye,
                           const glm::vec3& direction, const std::vector<Collider>& obstacles,
                           float weaponDistance)
{
    return safe && IsFirstTarget(terminal, eye, direction, obstacles, weaponDistance) && Open();
}

bool AnalysisSession::Open()
{
    if (active)
    {
        return false;
    }
    rotY = 35.f;
    rotX = 18.f;
    fov = 45.f;
    active = true;
    ++entries;
    return true;
}

bool AnalysisSession::Close()
{
    if (!active)
    {
        return false;
    }
    active = false;
    ++exits;
    return true;
}

void AnalysisSession::Orbit(float mouseX, float mouseY)
{
    if (!active || !std::isfinite(mouseX) || !std::isfinite(mouseY))
    {
        return;
    }
    const float sensitivity = 0.35f;
    rotY = std::fmod(rotY + mouseX * sensitivity, 360.f);
    rotX = std::clamp(rotX - mouseY * sensitivity, -60.f, 60.f);
}

void AnalysisSession::Zoom(double wheelSteps)
{
    if (active && std::isfinite(wheelSteps))
    {
        fov = static_cast<float>(std::clamp(fov - wheelSteps * 3.0, 35.0, 70.0));
    }
}

glm::vec3 AnalysisSession::EyePosition() const
{
    // 실습처럼 고정 반경의 카메라 위치만 피벗 주위로 회전한다.
    glm::mat4 orbit = glm::rotate(glm::mat4(1), glm::radians(rotY), glm::vec3(0, 1, 0));
    orbit = glm::rotate(orbit, glm::radians(-rotX), glm::vec3(1, 0, 0));
    return glm::vec3(orbit * glm::vec4(0, 0, 5.8f, 1));
}
} // namespace aegis::combat
