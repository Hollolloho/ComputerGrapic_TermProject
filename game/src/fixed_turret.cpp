// 제자리 포탑의 조준 경고와 사격 간격을 세계 시간으로 갱신한다.
#include "fixed_turret.h"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>
#include <limits>

namespace aegis::combat
{
namespace
{
bool Finite(const glm::vec3& v)
{
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}
} // namespace

bool FixedTurret::SetPosition(const glm::vec3& point)
{
    if (!Finite(point))
    {
        return false;
    }
    position = point;
    Reset();
    return true;
}

void FixedTurret::Reset()
{
    phase = TurretPhase::Idle;
    timer = 0;
    direction = {0, 0, 1};
    aimPoint = position + glm::vec3(0, 1.65f, 5);
}

void FixedTurret::Disarm()
{
    if (Alive())
    {
        phase = TurretPhase::Disarmed;
        timer = 0;
    }
}

void FixedTurret::Destroy()
{
    phase = TurretPhase::Destroyed;
    timer = 0;
}

glm::vec3 FixedTurret::Muzzle() const
{
    return position + glm::vec3(0, 1.65f, 0) + direction * 0.85f;
}

glm::mat4 FixedTurret::BarrelTransform() const
{
    const float yaw = std::atan2(direction.x, direction.z);
    const float pitch = -std::asin(std::clamp(direction.y, -1.0f, 1.0f));
    glm::mat4 transform = glm::translate(glm::mat4(1), position + glm::vec3(0, 1.65f, 0));
    transform = glm::rotate(transform, yaw, glm::vec3(0, 1, 0));
    transform = glm::rotate(transform, pitch, glm::vec3(1, 0, 0));
    return transform;
}

Collider FixedTurret::Body() const
{
    return {BodyId, HitKind::Body, position + glm::vec3(-0.5f, 0, -0.5f),
            position + glm::vec3(0.5f, 2.2f, 0.5f), Alive()};
}

Collider FixedTurret::Weapon() const
{
    // 총신의 여덟 꼭짓점을 변환하고, 월드 좌표의 충돌 상자를 구한다.
    const glm::vec4 corners[8] = {{-0.14f, -0.14f, -0.1f, 1}, {0.14f, -0.14f, -0.1f, 1},
                                  {-0.14f, 0.14f, -0.1f, 1},  {0.14f, 0.14f, -0.1f, 1},
                                  {-0.14f, -0.14f, 0.8f, 1},  {0.14f, -0.14f, 0.8f, 1},
                                  {-0.14f, 0.14f, 0.8f, 1},   {0.14f, 0.14f, 0.8f, 1}};
    glm::vec3 low(std::numeric_limits<float>::max());
    glm::vec3 high(std::numeric_limits<float>::lowest());
    const glm::mat4 transform = BarrelTransform();
    for (const glm::vec4& corner : corners)
    {
        glm::vec3 worldCorner = glm::vec3(transform * corner);
        low = glm::min(low, worldCorner);
        high = glm::max(high, worldCorner);
    }
    return {WeaponId, HitKind::Weapon, low, high, Armed()};
}

bool FixedTurret::Update(float gameDeltaTime, const glm::vec3& eye, std::span<const Collider> walls)
{
    if (!Armed() || !std::isfinite(gameDeltaTime) || gameDeltaTime <= 0 || !Finite(eye))
    {
        return false;
    }
    if (phase == TurretPhase::Cooldown)
    {
        timer -= gameDeltaTime;
        if (timer <= 0)
        {
            phase = TurretPhase::Idle;
        }
        return false;
    }
    const bool visible = glm::length(eye - position) < 18 && !FindFirstHit(Muzzle(), eye, walls);
    if (!visible)
    {
        phase = TurretPhase::Idle;
        timer = 0;
        return false;
    }
    if (phase == TurretPhase::Idle)
    {
        const auto delta = eye - (position + glm::vec3(0, 1.65f, 0));
        if (glm::dot(delta, delta) < 1e-8f)
        {
            return false;
        }
        direction = glm::normalize(delta);
        aimPoint = eye;
        phase = TurretPhase::Warning;
        timer = 1.2f;
        return false;
    }
    timer -= gameDeltaTime;
    if (timer > 0)
    {
        return false;
    }
    phase = TurretPhase::Cooldown;
    timer = 1.8f;
    return true;
}
} // namespace aegis::combat
