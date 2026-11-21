// 드론의 순찰 → 조준 경고 → 사격 → 대기와 무장 해제·파괴 상태를 관리한다.
#include "drone_behavior.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <glm/geometric.hpp>
#include <glm/trigonometric.hpp>

namespace aegis::combat
{
namespace
{
bool Finite(const glm::vec3& v)
{
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}

glm::vec3 Rotate(const glm::vec3& v, float yaw)
{
    const auto c = std::cos(yaw), s = std::sin(yaw);
    return {c * v.x + s * v.z, v.y, -s * v.x + c * v.z};
}
} // namespace

bool DroneBehavior::SetSpawn(const glm::vec3& point, float yawRadians, float endZ)
{
    if (!Finite(point) || !std::isfinite(yawRadians) || !std::isfinite(endZ))
    {
        return false;
    }
    spawn = point;
    spawnYaw = yawRadians;
    patrolEndZ = endZ;
    Reset();
    return true;
}

bool DroneBehavior::SetScale(float value)
{
    if (!std::isfinite(value) || value < .1f || value > 4)
    {
        return false;
    }
    scale = value;
    Reset();
    return true;
}

void DroneBehavior::Reset()
{
    position = spawn;
    aimPoint = {0, 0, 0};
    phase = DronePhase::Patrol;
    patrolYaw = spawnYaw;
    if (patrolEndZ < spawn.z)
        patrolYaw += glm::radians(180.f);
    yaw = patrolYaw;
    timer = 1.5f;
    movingToEnd = true;
    isTurning = false;
    turnDegrees = 0.f;
}

Collider DroneBehavior::Bounds(ObjectId id, HitKind kind, const glm::vec3& low, const glm::vec3& high) const
{
    // 상자의 여덟 꼭짓점을 모델과 똑같이 회전시킨 뒤, 전체를 감싸는 상자를 만든다.
    glm::vec3 corners[8] = {{low.x, low.y, low.z},   {high.x, low.y, low.z},  {low.x, high.y, low.z},
                            {high.x, high.y, low.z}, {low.x, low.y, high.z},  {high.x, low.y, high.z},
                            {low.x, high.y, high.z}, {high.x, high.y, high.z}};
    glm::vec3 minimum(std::numeric_limits<float>::max());
    glm::vec3 maximum(std::numeric_limits<float>::lowest());
    for (const glm::vec3& corner : corners)
    {
        glm::vec3 worldCorner = position + Rotate(corner * scale, yaw);
        minimum = glm::min(minimum, worldCorner);
        maximum = glm::max(maximum, worldCorner);
    }
    return {id, kind, minimum, maximum, phase != DronePhase::Destroyed};
}

Collider DroneBehavior::Body() const
{
    return Bounds(500, HitKind::Body, {-1.385f, -0.3f, -0.475f}, {1.385f, 0.515f, 0.664f});
}

Collider DroneBehavior::Weapon() const
{
    auto box = Bounds(501, HitKind::Weapon, {-0.11f, -0.505f, 0.03f}, {0.11f, -0.275f, 0.655f});
    box.enabled = Armed();
    return box;
}

glm::vec3 DroneBehavior::Muzzle() const
{
    return position + Rotate(glm::vec3(0, -0.35f, 0.68f) * scale, yaw);
}

bool DroneBehavior::Visible(const glm::vec3& eye, std::span<const Collider> walls) const
{
    return glm::length(eye - position) < 18 && !FindFirstHit(Muzzle(), eye, walls);
}

void DroneBehavior::MovingDrone(float gameDeltaTime, std::span<const Collider> walls)
{
    // 조준이 끝나면 멈췄던 순찰 방향과 회전 각도에서 이어간다.
    yaw = patrolYaw;
    if (patrolEndZ == spawn.z)
        return;

    if (isTurning)
    {
        turnDegrees = std::min(180.f, turnDegrees + 60.f * gameDeltaTime);
        yaw += glm::radians(turnDegrees);
        if (turnDegrees >= 180.f)
        {
            patrolYaw = std::remainder(yaw, glm::radians(360.f));
            movingToEnd = !movingToEnd;
            isTurning = false;
            turnDegrees = 0.f;
        }
        return;
    }

    const float targetZ = movingToEnd ? patrolEndZ : spawn.z;
    const float remaining = targetZ - position.z;
    const float distance = 1.2f * gameDeltaTime;
    glm::vec3 next = position;
    if (std::abs(remaining) <= distance)
        next.z = targetZ;
    else
        next.z += std::copysign(distance, remaining);

    // 몸통과 무기의 크기까지 포함해 이번 이동 구간에 벽이 있는지 검사한다.
    const Collider body = Body();
    const Collider weapon = Weapon();
    const glm::vec3 low = glm::min(body.minimum, weapon.minimum) - position;
    const glm::vec3 high = glm::max(body.maximum, weapon.maximum) - position;
    for (const Collider& wall : walls)
    {
        Collider expandedWall = wall;
        expandedWall.minimum -= high;
        expandedWall.maximum -= low;
        float fraction = 0;
        if (SegmentHitsBox(position, next, expandedWall, fraction))
        {
            isTurning = true;
            return;
        }
    }
    position = next;
    isTurning = position.z == targetZ;
}

bool DroneBehavior::DetectPlayer(const glm::vec3& eye, std::span<const Collider> walls) const
{
    return false; // 정면 시야 판정을 작성하기 전의 임시 반환값
}

bool DroneBehavior::Update(float gameDeltaTime, const glm::vec3& eye, std::span<const Collider> walls)
{
    if (!std::isfinite(gameDeltaTime) || gameDeltaTime <= 0 || !Finite(eye))
    {
        return false;
    }

    // 반환값 true는 "이번 프레임에 한 발 발사"라는 뜻이다.
    switch (phase)
    {
    case DronePhase::Patrol:
        MovingDrone(gameDeltaTime, walls);
        timer = std::max(0.0f, timer - gameDeltaTime);
        if (timer <= 0 && Visible(eye, walls))
        {
            phase = DronePhase::Tracking;
        }
        break;

    case DronePhase::Tracking:
    {
        const glm::vec3 direction = eye - position;
        if (direction.x * direction.x + direction.z * direction.z < 0.000001f)
            break;

        const float targetYaw = std::atan2(direction.x, direction.z);
        // +179도에서 -179도를 볼 때도 짧은 방향으로 회전한다.
        const float angleDiff = std::remainder(targetYaw - yaw, glm::radians(360.0f));
        const float maxStep = glm::radians(90.0f) * gameDeltaTime;
        yaw += std::clamp(angleDiff, -maxStep, maxStep);
        break;
    }
    case DronePhase::Warning:
        if (!Visible(eye, walls))
        {
            phase = DronePhase::Patrol;
            timer = 0;
            break;
        }
        timer -= gameDeltaTime;
        if (timer <= 0)
        {
            phase = DronePhase::Waiting;
            timer = 1.5f;
            return true;
        }
        break;

    case DronePhase::Waiting:
        timer -= gameDeltaTime;
        if (timer <= 0)
        {
            phase = DronePhase::Patrol;
            timer = 0;
        }
        break;

    case DronePhase::Disarmed:
    case DronePhase::Destroyed:
        // 무기가 없거나 파괴된 드론은 이동·사격하지 않는다.
        break;
    }
    return false;
}

void DroneBehavior::Disarm()
{
    if (phase != DronePhase::Destroyed)
    {
        phase = DronePhase::Disarmed;
        timer = 0;
    }
}

void DroneBehavior::Destroy()
{
    phase = DronePhase::Destroyed;
    timer = 0;
}
} // namespace aegis::combat
