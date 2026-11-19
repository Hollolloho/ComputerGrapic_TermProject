// 드론의 순찰 → 조준 경고 → 사격 → 대기와 무장 해제·파괴 상태를 관리한다.
#include "drone_behavior.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <glm/geometric.hpp>

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

bool DroneBehavior::SetSpawn(const glm::vec3& point, float yawRadians, float patrolCenterX)
{
    if (!Finite(point) || !std::isfinite(yawRadians) || !std::isfinite(patrolCenterX) ||
        std::abs(point.x - patrolCenterX) > 0.65f)
    {
        return false;
    }
    spawn = point;
    spawnYaw = yawRadians;
    patrolCenter = patrolCenterX;
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
    yaw = spawnYaw;
    timer = 1.5f;
    movingRight = true;
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

bool DroneBehavior::Move(float gameDeltaTime, std::span<const Collider> walls)
{
    // 중심에서 좌우 0.65만큼 왕복한다. 속도 × 시간으로 이번 이동 거리를 구한다.
    const double leftEnd = -0.65;
    const double rightEnd = 0.65;
    const double routeLength = rightEnd - leftEnd;
    const double travelDistance = 0.65 * gameDeltaTime;
    double nextX = position.x - patrolCenter;
    bool nextMovingRight = movingRight;

    // 한 바퀴를 돌아오면 위치가 같으므로, 긴 프레임도 끝없이 반복하지 않는다.
    double distanceLeft = std::fmod(travelDistance, routeLength * 2);
    double lowestX = nextX;
    double highestX = nextX;
    while (distanceLeft > 0)
    {
        double targetX = leftEnd;
        if (nextMovingRight)
        {
            targetX = rightEnd;
        }
        double distanceToEnd = std::abs(targetX - nextX);
        double step = std::min(distanceLeft, distanceToEnd);
        if (nextMovingRight)
        {
            nextX += step;
        }
        else
        {
            nextX -= step;
        }
        distanceLeft -= step;
        lowestX = std::min(lowestX, nextX);
        highestX = std::max(highestX, nextX);
        if (step == distanceToEnd)
        {
            nextMovingRight = !nextMovingRight;
        }
    }
    if (travelDistance >= routeLength)
    {
        lowestX = leftEnd;
        highestX = rightEnd;
    }

    // 드론 크기만큼 벽을 넓혀 검사하면, 중심뿐 아니라 날개도 벽을 통과하지 않는다.
    const Collider body = Body();
    const Collider weapon = Weapon();
    const glm::vec3 localMinimum = glm::min(body.minimum, weapon.minimum) - position;
    const glm::vec3 localMaximum = glm::max(body.maximum, weapon.maximum) - position;
    glm::vec3 pathStart = position;
    glm::vec3 pathEnd = position;
    pathStart.x = patrolCenter + static_cast<float>(lowestX);
    pathEnd.x = patrolCenter + static_cast<float>(highestX);
    for (const Collider& wall : walls)
    {
        Collider expandedWall = wall;
        expandedWall.minimum -= localMaximum;
        expandedWall.maximum -= localMinimum;
        float hitFraction = 0;
        if (SegmentHitsBox(pathStart, pathEnd, expandedWall, hitFraction))
        {
            movingRight = !movingRight;
            return false;
        }
    }
    position.x = patrolCenter + static_cast<float>(nextX);
    movingRight = nextMovingRight;
    return true;
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
        yaw = spawnYaw;
        Move(gameDeltaTime, walls);
        timer = std::max(0.0f, timer - gameDeltaTime);
        if (timer <= 0 && Visible(eye, walls))
        {
            // 발견 순간의 위치를 기억한다. 경고 중에 계속 플레이어를 따라 조준하지 않는다.
            aimPoint = eye;
            glm::vec3 direction = eye - position;
            yaw = std::atan2(direction.x, direction.z);
            phase = DronePhase::Warning;
            timer = 1.0f;
        }
        break;

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
