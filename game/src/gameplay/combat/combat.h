// 충돌 결과로 피해를 적용하는 곳은 encounter 파일이다.
#pragma once

#include <cstdint>
#include <vector>
#include <glm/vec3.hpp>

namespace aegis::combat
{

using ObjectId = std::uint32_t;

// Equal-distance ties prefer blocking surfaces, then weapons, then bodies.
enum class HitKind
{
    Wall,
    Armor,
    Weapon,
    Body
};

struct Collider
{
    ObjectId id{};
    HitKind kind{HitKind::Wall};
    glm::vec3 minimum{0.0f};
    glm::vec3 maximum{0.0f};
    bool enabled{true};
};

Collider PlayerBodyCollider(ObjectId id, const glm::vec3& eye);

struct Hit
{
    ObjectId objectId{};
    HitKind kind{HitKind::Wall};
    float fraction{}; // 0..1 along this frame's entire movement segment.
    glm::vec3 position{0.0f};
};

struct Bullet
{
    glm::vec3 position{0.0f};
    glm::vec3 velocity{0.0f}; // World units per game-time second.
    ObjectId sourceWeaponId{};
    bool active{true};
};

const float BulletLifetime = 4.f;
const float TrailMoteSpacing = 0.18f;
const float TrailMoteLifetime = 0.30f;
const float TrailLifetime = 0.16f;

struct Flight
{
    Bullet bullet;
    float age{};
    double distanceToMote{TrailMoteSpacing};
    unsigned int moteIndex{};
};

struct CrystalTrail
{
    glm::vec3 end{}, velocity{};
    float flightTime{};
    float age{};
};

struct CrystalMote
{
    glm::vec3 position{};
    float age{};
};

void RemoveExpiredFlights(std::vector<Flight>& flights);

// Rejects non-finite inputs, zero direction and non-positive speed.
bool MakeBullet(const glm::vec3& origin, const glm::vec3& direction, float speed,
                ObjectId sourceWeaponId, Bullet& bullet);

glm::vec3 PointOnSegment(const glm::vec3& start, const glm::vec3& end, float fraction);

// 상자 하나와 충돌하면 hitFraction에 선분의 충돌 비율(0~1)을 넣는다.
bool SegmentHitsBox(const glm::vec3& start, const glm::vec3& end, const Collider& box, float& hitFraction);

// Axis-aligned boxes in world coordinates. Disabled/invalid boxes are skipped.
// A segment starting in/on a box hits at fraction 0. Ties use HitKind, then ID.
bool FindFirstHit(const glm::vec3& start, const glm::vec3& end,
                  const std::vector<Collider>& colliders, Hit& hit);
bool IsBlocked(const glm::vec3& start, const glm::vec3& end, const std::vector<Collider>& colliders);

// Caller supplies the shared gameDeltaTime (not raw wall-clock dt).
// Stops/deactivates at the first hit; damage and weapon state belong to caller.
// No work for inactive bullets, paused time or invalid dt.
bool AdvanceBullet(Bullet& bullet, float gameDeltaTime,
                   const std::vector<Collider>& colliders, Hit& hit);

} // namespace aegis::combat
