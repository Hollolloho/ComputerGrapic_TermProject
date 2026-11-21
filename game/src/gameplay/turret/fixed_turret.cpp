// 제자리 포탑의 조준 경고와 사격 간격을 세계 시간으로 갱신한다.
#include "fixed_turret.h"
#include "main/GameMath.h"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>
#include <limits>

namespace aegis::combat
{
namespace
{
const float pivotHeight = 1.62f;
const float barrelReach = 1.115f;
const float twinBarrelOffset = 0.57f;
} // namespace

bool FixedTurret::SetPosition(const glm::vec3& point)
{
    if (!IsFinite(point))
    {
        return false;
    }
    position = point;
    Reset();
    return true;
}

void FixedTurret::Configure(TurretType model, const glm::vec3& point, ObjectId index, float rotYDegrees)
{
    type = model;
    number = index;
    spawnRotY = glm::radians(rotYDegrees);
    SetPosition(point);
}

void FixedTurret::Reset()
{
    phase = TurretPhase::Idle;
    timer = 0;
    direction = RotateY({0, 0, 1}, spawnRotY);
    firingBarrel = 0;
    aimPoint = position + glm::vec3(0, pivotHeight, 0) + direction * 5.0f;
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

float FixedTurret::BarrelOffset(int barrel) const
{
    if (type == TurretType::SingleC) return 0;
    return barrel == 0 ? -twinBarrelOffset : twinBarrelOffset;
}

glm::vec3 FixedTurret::Muzzle() const
{
    return glm::vec3(BarrelTransform() * glm::vec4(BarrelOffset(firingBarrel), 0, barrelReach, 1));
}

glm::mat4 FixedTurret::BarrelTransform() const
{
    const float rotY = std::atan2(direction.x, direction.z);
    const float rotX = -std::asin(std::clamp(direction.y, -1.0f, 1.0f));
    glm::mat4 transform = glm::translate(glm::mat4(1), position + glm::vec3(0, pivotHeight, 0));
    transform = glm::rotate(transform, rotY, glm::vec3(0, 1, 0));
    transform = glm::rotate(transform, rotX, glm::vec3(1, 0, 0));
    return transform;
}

Collider FixedTurret::Body() const
{
    const float halfWidth = type == TurretType::Twin ? 0.925f : 0.81f;
    const float height = type == TurretType::Twin ? 2.118f : 2.18f;
    return {BodyTarget(), HitKind::Body, position + glm::vec3(-halfWidth, 0, -halfWidth),
            position + glm::vec3(halfWidth, height, halfWidth), Alive()};
}

Collider FixedTurret::Weapon(int barrel) const
{
    // 총신의 여덟 꼭짓점을 변환하고, 월드 좌표의 충돌 상자를 구한다.
    const float front = barrelReach - 0.05f;
    const glm::vec4 corners[8] = {{-0.14f, -0.14f, -0.1f, 1}, {0.14f, -0.14f, -0.1f, 1},
                                  {-0.14f, 0.14f, -0.1f, 1},  {0.14f, 0.14f, -0.1f, 1},
                                  {-0.14f, -0.14f, front, 1},  {0.14f, -0.14f, front, 1},
                                  {-0.14f, 0.14f, front, 1},   {0.14f, 0.14f, front, 1}};
    glm::vec3 low(std::numeric_limits<float>::max());
    glm::vec3 high(std::numeric_limits<float>::lowest());
    const glm::mat4 transform = glm::translate(BarrelTransform(), glm::vec3(BarrelOffset(barrel), 0, 0));
    for (const glm::vec4& corner : corners)
    {
        glm::vec3 worldCorner = glm::vec3(transform * corner);
        low = glm::min(low, worldCorner);
        high = glm::max(high, worldCorner);
    }
    return {WeaponTarget(), HitKind::Weapon, low, high, Armed() && (barrel == 0 || type == TurretType::Twin)};
}

bool FixedTurret::Update(float gameDeltaTime, const glm::vec3& eye, std::span<const Collider> walls)
{
    if (!Armed() || !std::isfinite(gameDeltaTime) || gameDeltaTime <= 0 || !IsFinite(eye))
    {
        return false;
    }
    if (phase == TurretPhase::Cooldown)
    {
        timer -= gameDeltaTime;
        if (timer <= 0)
        {
            phase = TurretPhase::Idle;
            firingBarrel = 1 - firingBarrel;
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
        const auto delta = eye - (position + glm::vec3(0, pivotHeight, 0));
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
