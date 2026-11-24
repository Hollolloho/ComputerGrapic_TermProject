// 드론의 순찰 → 조준 경고 → 사격 → 대기와 무장 해제·파괴 상태를 관리한다.
#include "drone_behavior.h"
#include "main/GameMath.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <glm/geometric.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/trigonometric.hpp>

namespace aegis::combat
{
bool DroneBehavior::SetSpawn(const glm::vec3& point, float rotYRadians, float endZ)
{
    if (!IsFinite(point) || !std::isfinite(rotYRadians) || !std::isfinite(endZ))
    {
        return false;
    }
    spawn = point;
    spawnRotY = rotYRadians;
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
    patrolRotY = spawnRotY;
    if (patrolEndZ < spawn.z)
        patrolRotY += glm::radians(180.f);
    rotY = patrolRotY;
    timer = 1.5;
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
        glm::vec3 worldCorner = position + RotateY(corner * scale, rotY);
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

glm::mat4 DroneBehavior::WorldTransform() const
{
    glm::mat4 world = glm::translate(glm::mat4(1), position);
    world = glm::rotate(world, rotY, glm::vec3(0, 1, 0));
    return glm::scale(world, glm::vec3(scale));
}

glm::vec3 DroneBehavior::Muzzle() const
{
    return position + RotateY(glm::vec3(0, -0.35f, 0.68f) * scale, rotY);
}

bool DroneBehavior::Visible(const glm::vec3& eye, const std::vector<Collider>& walls) const
{
    return glm::length(eye - position) < 18 && !IsBlocked(Muzzle(), eye, walls);
}

bool DroneBehavior::TurnToward(const glm::vec3& eye, float gameDeltaTime)
{
    const glm::vec3 direction = eye - position;
    if (direction.x * direction.x + direction.z * direction.z < 0.000001f)
        return false;

    const float targetRotY = std::atan2(direction.x, direction.z);
    // +179도에서 -179도로 바뀌어도 짧은 방향으로 회전한다.
    const float angleDiff = std::remainder(targetRotY - rotY, glm::radians(360.0f));
    const float maxStep = glm::radians(90.0f) * gameDeltaTime;
    rotY += std::clamp(angleDiff, -maxStep, maxStep);
    return std::abs(angleDiff) <= maxStep;
}

void DroneBehavior::MovingDrone(float gameDeltaTime, const std::vector<Collider>& walls)
{
    // 조준이 끝나면 멈췄던 순찰 방향과 회전 각도에서 이어간다.
    rotY = patrolRotY;
    if (patrolEndZ == spawn.z)
        return;

    if (isTurning)
    {
        turnDegrees = std::min(180.f, turnDegrees + 60.f * gameDeltaTime);
        rotY += glm::radians(turnDegrees);
        if (turnDegrees >= 180.f)
        {
            patrolRotY = std::remainder(rotY, glm::radians(360.f));
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

bool DroneBehavior::DetectPlayer(const glm::vec3& eye, const std::vector<Collider>& walls) const
{
    const glm::vec3 sensor_offset(0.f, -0.015f, 0.49f);
    const glm::vec3 sensor_pos = position + RotateY(sensor_offset * scale, rotY);
    const glm::vec3 to_player = eye - sensor_pos;

    const float distance = glm::length(to_player);

    if (distance < 0.0001f || distance >= 18.f)
        return false;
    // 모델의 정면(+Z)을 현재 회전각만큼 돌려 렌즈가 보는 방향을 구한다.
    const glm::vec3 forward = RotateY(glm::vec3(0.f, 0.f, 1.f), rotY);
    const glm::vec3 player_dir = to_player / distance;

    const float facing = glm::dot(forward, player_dir);

    // 드론 정면에서 45도보다 벗어나 있으면 제외
    if (facing < std::cos(glm::radians(45.f)))
        return false;

    return !IsBlocked(sensor_pos, eye, walls);
}

bool DroneBehavior::Update(float gameDeltaTime, const glm::vec3& eye, const std::vector<Collider>& walls)
{
    if (!std::isfinite(gameDeltaTime) || gameDeltaTime <= 0 || !IsFinite(eye))
    {
        return false;
    }

    // 반환값 true는 "이번 프레임에 한 발 발사"라는 뜻이다.
    switch (phase)
    {
    case DronePhase::Patrol:
        MovingDrone(gameDeltaTime, walls);
        timer = std::max(0.0, timer - gameDeltaTime);
        if (timer <= 0 && DetectPlayer(eye, walls))
        {
            phase = DronePhase::Tracking;
        }
        break;

    case DronePhase::Tracking:
    case DronePhase::Waiting:
    {
        const bool visible = Visible(eye, walls);
        const bool aligned = visible && TurnToward(eye, gameDeltaTime);
        if (phase == DronePhase::Waiting)
        {
            timer = std::max(0.0, timer - gameDeltaTime);
            if (timer > 0)
                break;
        }

        phase = visible ? DronePhase::Tracking : DronePhase::Patrol;
        // 회전한 총구에서도 사선이 열려 있을 때 조준점을 고정한다.
        if (aligned && Visible(eye, walls))
        {
            phase = DronePhase::Warning;
            timer = 1.0;
            aimPoint = eye;
        }
        break;
    }
    case DronePhase::Warning:
        // 현재 플레이어와 예고 때 고정한 조준점의 사선을 모두 확인한다.
        if (!Visible(eye, walls) || !Visible(aimPoint, walls))
        {
            phase = DronePhase::Patrol;
            timer = 0;
            break;
        }

        timer -= gameDeltaTime;

        if (timer <= 0)
        {
            phase = DronePhase::Waiting;
            timer = 1.5;
            return true;
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
