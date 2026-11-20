// span은 배열·벡터를 복사하지 않고 읽는다. optional은 결과가 없을 수 있음을 뜻한다.
// 충돌 결과로 피해를 적용하는 곳은 encounter 파일이다.
#pragma once

#include <cstdint>
#include <optional>
#include <span>
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

struct Hit
{
    ObjectId objectId{};
    HitKind kind{HitKind::Wall};
    float fraction{}; // 0..1 along this frame's entire movement segment.
    glm::vec3 position{0.0f};
};

struct Projectile
{
    glm::vec3 position{0.0f};
    glm::vec3 velocity{0.0f}; // World units per game-time second.
    ObjectId sourceWeaponId{};
    bool active{true};
};

// Rejects non-finite inputs, zero direction and non-positive speed.
std::optional<Projectile> MakeProjectile(const glm::vec3& origin, const glm::vec3& direction, float speed,
                                         ObjectId sourceWeaponId);

// 상자 하나와의 충돌 검사. 충돌하면 true, hitFraction에 선분의 충돌 비율(0~1)을 넣는다.
bool SegmentHitsBox(const glm::vec3& start, const glm::vec3& end, const Collider& box, float& hitFraction);

// Axis-aligned boxes in world coordinates. Disabled/invalid boxes are skipped.
// A segment starting in/on a box hits at fraction 0. Ties use HitKind, then ID.
std::optional<Hit> FindFirstHit(const glm::vec3& start, const glm::vec3& end,
                                std::span<const Collider> colliders);

// Caller supplies the shared gameDeltaTime (not raw wall-clock dt).
// Stops/deactivates at the first hit; damage and weapon state belong to caller.
// No work for inactive projectiles, paused time or invalid dt.
std::optional<Hit> AdvanceProjectile(Projectile& projectile, float gameDeltaTime,
                                     std::span<const Collider> colliders);

} // namespace aegis::combat
