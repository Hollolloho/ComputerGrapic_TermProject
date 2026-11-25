// 탄환의 이전 위치와 다음 위치 사이에서 가장 가까운 충돌을 찾는다.
#include "combat.h"
#include "gameplay/player/player_settings.h"
#include "main/GameMath.h"

#include <algorithm>
#include <cmath>
#include <glm/geometric.hpp>

namespace aegis::combat
{

Collider PlayerBodyCollider(ObjectId id, const glm::vec3& eye)
{
    const float floorY = eye.y - player::EyeHeight;
    return {id, HitKind::Body,
            {eye.x - player::Radius, floorY, eye.z - player::Radius},
            {eye.x + player::Radius, floorY + player::BodyHeight, eye.z + player::Radius}};
}

void RemoveExpiredFlights(std::vector<Flight>& flights)
{
    // 살아 있는 탄환을 원래 순서대로 앞쪽에 모은다.
    std::size_t kept = 0;
    for (const Flight& flight : flights)
    {
        if (!flight.bullet.active || flight.age >= BulletLifetime)
            continue;
        flights[kept] = flight;
        ++kept;
    }
    flights.resize(kept);
}

glm::vec3 PointOnSegment(const glm::vec3& start, const glm::vec3& end, float fraction)
{
    glm::vec3 point;
    for (int axis = 0; axis < 3; ++axis)
    {
        const double movement = static_cast<double>(end[axis]) - start[axis];
        point[axis] = static_cast<float>(start[axis] + movement * fraction);
    }
    return point;
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

bool MakeBullet(const glm::vec3& origin, const glm::vec3& direction, float speed,
                ObjectId sourceWeaponId, Bullet& bullet)
{
    if (!IsFinite(origin) || !IsFinite(direction) || !std::isfinite(speed) || speed <= 0)
    {
        return false;
    }
    const double length = std::sqrt(static_cast<double>(direction.x) * direction.x +
                                    static_cast<double>(direction.y) * direction.y +
                                    static_cast<double>(direction.z) * direction.z);
    if (length == 0.0)
    {
        return false;
    }
    const glm::vec3 velocity{static_cast<float>(direction.x / length * speed),
                             static_cast<float>(direction.y / length * speed),
                             static_cast<float>(direction.z / length * speed)};
    bullet = {origin, velocity, sourceWeaponId, true};
    return true;
}

bool FindFirstHit(const glm::vec3& start, const glm::vec3& end,
                  const std::vector<Collider>& colliders, Hit& nearestHit)
{
    bool found = false;
    for (const Collider& box : colliders)
    {
        float hitFraction = 0;
        if (!SegmentHitsBox(start, end, box, hitFraction))
        {
            continue;
        }

        // 같은 거리라면 벽 → 장갑 → 무기 → 본체 순서로 우선한다.
        if (found && hitFraction > nearestHit.fraction)
            continue;
        if (found && hitFraction == nearestHit.fraction)
        {
            if (box.kind > nearestHit.kind)
                continue;
            if (box.kind == nearestHit.kind && box.id >= nearestHit.objectId)
                continue;
        }
        found = true;
        nearestHit.objectId = box.id;
        nearestHit.kind = box.kind;
        nearestHit.fraction = hitFraction;
    }
    if (!found)
    {
        return false;
    }

    nearestHit.position = PointOnSegment(start, end, nearestHit.fraction);
    return true;
}

bool IsBlocked(const glm::vec3& start, const glm::vec3& end, const std::vector<Collider>& colliders)
{
    for (const Collider& box : colliders)
    {
        float fraction;
        if (SegmentHitsBox(start, end, box, fraction))
            return true;
    }
    return false;
}

bool AdvanceBullet(Bullet& bullet, float gameDeltaTime,
                   const std::vector<Collider>& colliders, Hit& hit)
{
    if (!bullet.active || !std::isfinite(gameDeltaTime) || gameDeltaTime <= 0 ||
        !IsFinite(bullet.position) || !IsFinite(bullet.velocity))
    {
        return false;
    }
    // 위치 = 이전 위치 + 속도 × 경과시간. 두 위치 사이도 검사해 얇은 벽을 건너뛰지 않는다.
    const glm::vec3 next = bullet.position + bullet.velocity * gameDeltaTime;
    if (!IsFinite(next))
    {
        return false;
    }
    if (FindFirstHit(bullet.position, next, colliders, hit))
    {
        bullet.position = hit.position;
        bullet.active = false;
        return true;
    }
    bullet.position = next;
    return false;
}

} // namespace aegis::combat
