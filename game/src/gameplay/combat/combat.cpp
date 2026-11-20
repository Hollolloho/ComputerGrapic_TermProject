// 탄환의 이전 위치와 다음 위치 사이에서 가장 가까운 충돌을 찾는다.
#include "combat.h"

#include <algorithm>
#include <cmath>
#include <glm/geometric.hpp>

namespace aegis::combat
{

bool IsFinite(const glm::vec3& value)
{
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

void RemoveExpiredFlights(std::vector<Flight>& flights)
{
    std::erase_if(flights, [](const Flight& flight)
                  { return !flight.projectile.active || flight.age >= 4.0f; });
}

// 선분에서 0은 출발점, 1은 도착점이다. 세 축에서 상자 안에 있는 구간을 겹쳐 본다.
bool SegmentHitsBox(const glm::vec3& start, const glm::vec3& end, const Collider& box, float& hitFraction)
{
    if (!box.enabled || !IsFinite(start) || !IsFinite(end) || !IsFinite(box.minimum) ||
        !IsFinite(box.maximum))
    {
        return false;
    }
    double enter = 0.0;
    double leave = 1.0;
    for (int axis = 0; axis < 3; ++axis)
    {
        if (box.minimum[axis] > box.maximum[axis])
        {
            return false;
        }
        double movement = static_cast<double>(end[axis]) - start[axis];
        if (movement == 0)
        {
            // 이 축으로 이동하지 않는데 이미 상자 밖이라면 충돌할 수 없다.
            if (start[axis] < box.minimum[axis] || start[axis] > box.maximum[axis])
            {
                return false;
            }
            continue;
        }
        double nearTime = (static_cast<double>(box.minimum[axis]) - start[axis]) / movement;
        double farTime = (static_cast<double>(box.maximum[axis]) - start[axis]) / movement;
        if (nearTime > farTime)
        {
            std::swap(nearTime, farTime);
        }
        enter = std::max(enter, nearTime);
        leave = std::min(leave, farTime);
        if (enter > leave)
        {
            return false;
        }
    }
    hitFraction = static_cast<float>(enter);
    return true;
}

std::optional<Projectile> MakeProjectile(const glm::vec3& origin, const glm::vec3& direction, float speed,
                                         ObjectId sourceWeaponId)
{
    if (!IsFinite(origin) || !IsFinite(direction) || !std::isfinite(speed) || speed <= 0)
    {
        return std::nullopt;
    }
    const double length = std::sqrt(static_cast<double>(direction.x) * direction.x +
                                    static_cast<double>(direction.y) * direction.y +
                                    static_cast<double>(direction.z) * direction.z);
    if (length == 0.0)
    {
        return std::nullopt;
    }
    const glm::vec3 velocity{static_cast<float>(direction.x / length * speed),
                             static_cast<float>(direction.y / length * speed),
                             static_cast<float>(direction.z / length * speed)};
    return Projectile{origin, velocity, sourceWeaponId, true};
}

std::optional<Hit> FindFirstHit(const glm::vec3& start, const glm::vec3& end,
                                std::span<const Collider> colliders)
{
    bool found = false;
    Hit nearestHit;
    for (const Collider& box : colliders)
    {
        float hitFraction = 0;
        if (!SegmentHitsBox(start, end, box, hitFraction))
        {
            continue;
        }

        // 같은 거리라면 벽 → 장갑 → 무기 → 본체 순서로 우선한다.
        bool useThisHit = !found;
        if (found && hitFraction < nearestHit.fraction)
        {
            useThisHit = true;
        }
        if (found && hitFraction == nearestHit.fraction)
        {
            if (box.kind < nearestHit.kind)
            {
                useThisHit = true;
            }
            else if (box.kind == nearestHit.kind && box.id < nearestHit.objectId)
            {
                useThisHit = true;
            }
        }
        if (useThisHit)
        {
            found = true;
            nearestHit.objectId = box.id;
            nearestHit.kind = box.kind;
            nearestHit.fraction = hitFraction;
        }
    }
    if (!found)
    {
        return std::nullopt;
    }

    // 출발점에서 가장 가까운 충돌 위치를 구한다. 중간 계산은 double로 한다.
    for (int axis = 0; axis < 3; ++axis)
    {
        double movement = static_cast<double>(end[axis]) - start[axis];
        nearestHit.position[axis] = static_cast<float>(start[axis] + movement * nearestHit.fraction);
    }
    return nearestHit;
}

std::optional<Hit> AdvanceProjectile(Projectile& projectile, float gameDeltaTime,
                                     std::span<const Collider> colliders)
{
    if (!projectile.active || !std::isfinite(gameDeltaTime) || gameDeltaTime <= 0 ||
        !IsFinite(projectile.position) || !IsFinite(projectile.velocity))
    {
        return std::nullopt;
    }
    // 위치 = 이전 위치 + 속도 × 경과시간. 두 위치 사이도 검사해 얇은 벽을 건너뛰지 않는다.
    const glm::vec3 next = projectile.position + projectile.velocity * gameDeltaTime;
    if (!IsFinite(next))
    {
        return std::nullopt;
    }
    const auto hit = FindFirstHit(projectile.position, next, colliders);
    if (hit)
    {
        projectile.position = hit->position;
        projectile.active = false;
    }
    else
    {
        projectile.position = next;
    }
    return hit;
}

} // namespace aegis::combat
