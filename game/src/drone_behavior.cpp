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
    destination = 1;
}

Collider DroneBehavior::Bounds(ObjectId id, HitKind kind, const glm::vec3& low, const glm::vec3& high) const
{
    glm::vec3 minimum(std::numeric_limits<float>::max()), maximum(std::numeric_limits<float>::lowest());
    for (int i = 0; i < 8; ++i)
    {
        const auto p =
            position +
            Rotate(glm::vec3(i & 1 ? high.x : low.x, i & 2 ? high.y : low.y, i & 4 ? high.z : low.z) * scale,
                   yaw);
        minimum = glm::min(minimum, p);
        maximum = glm::max(maximum, p);
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

bool DroneBehavior::Move(float dt, std::span<const Collider> walls)
{
    // Analytic ping-pong keeps speed independent of frame rate at turnarounds.
    constexpr double span = 1.3, period = 2.6;
    const double progress =
        destination ? position.x - patrolCenter + 0.65 : span + (0.65 - (position.x - patrolCenter));
    const double travel = 0.65 * dt;
    const double p = std::fmod(progress + travel, period);
    auto next = position;
    next.x = patrolCenter + static_cast<float>(-0.65 + (p < span ? p : period - p));
    const auto body = Body();
    const auto weapon = Weapon();
    const auto low = glm::min(body.minimum, weapon.minimum) - position;
    const auto high = glm::max(body.maximum, weapon.maximum) - position;
    auto start = position, end = next;
    const double boundary = destination ? span - progress : period - progress;
    if (travel >= span)
    {
        start.x = patrolCenter - 0.65f;
        end.x = patrolCenter + 0.65f;
    }
    else if (travel > boundary)
    {
        start.x = destination ? std::min(position.x, next.x) : std::max(position.x, next.x);
        end.x = patrolCenter + (destination ? 0.65f : -0.65f);
    }
    for (const auto& wall : walls)
    {
        if (!wall.enabled)
        {
            continue;
        }
        const Collider expanded{wall.id, HitKind::Wall, wall.minimum - high, wall.maximum - low};
        if (FindFirstHit(start, end, std::span<const Collider>(&expanded, 1)))
        {
            destination = 1 - destination;
            return false;
        }
    }
    position = next;
    destination = p < span ? 1 : 0;
    return true;
}

bool DroneBehavior::Update(float gameDeltaTime, const glm::vec3& eye, std::span<const Collider> walls)
{
    if (!std::isfinite(gameDeltaTime) || gameDeltaTime <= 0 || !Finite(eye) || !Armed())
    {
        return false;
    }
    const float dt = gameDeltaTime;
    if (phase == DronePhase::Patrol)
    {
        yaw = spawnYaw;
        Move(dt, walls);
        timer = std::max(0.0f, timer - dt);
        if (timer <= 0 && Visible(eye, walls))
        {
            phase = DronePhase::Warning;
            timer = 1;
            aimPoint = eye;
            const auto direction = eye - position;
            yaw = std::atan2(direction.x, direction.z);
        }
        return false;
    }
    if (phase == DronePhase::Waiting)
    {
        timer -= dt;
        if (timer <= 0)
        {
            phase = DronePhase::Patrol;
            timer = 0;
        }
        return false;
    }
    if (!Visible(eye, walls))
    {
        phase = DronePhase::Patrol;
        timer = 0;
        return false;
    }
    timer -= dt;
    if (timer > 0)
    {
        return false;
    }
    phase = DronePhase::Waiting;
    timer = 1.5f;
    return true;
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
