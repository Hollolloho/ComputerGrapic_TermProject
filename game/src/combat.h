#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <glm/vec3.hpp>

namespace aegis::combat {

using ObjectId = std::uint32_t;

// Equal-distance ties prefer blocking surfaces, then weapons, then bodies.
enum class HitKind { Wall, Armor, Weapon, Body };

struct Collider {
    ObjectId id{};
    HitKind kind{HitKind::Wall};
    glm::vec3 minimum{0.0f};
    glm::vec3 maximum{0.0f};
    bool enabled{true};
};

struct Hit {
    ObjectId objectId{};
    HitKind kind{HitKind::Wall};
    float fraction{}; // 0..1 along this frame's entire movement segment.
    glm::vec3 position{0.0f};
};

struct Projectile {
    glm::vec3 position{0.0f};
    glm::vec3 velocity{0.0f}; // World units per game-time second.
    ObjectId sourceWeaponId{};
    bool active{true};
};

// Rejects non-finite inputs, zero direction and non-positive speed.
std::optional<Projectile> MakeProjectile(const glm::vec3& origin,
    const glm::vec3& direction, float speed, ObjectId sourceWeaponId);

// Axis-aligned boxes in world coordinates. Disabled/invalid boxes are skipped.
// A segment starting in/on a box hits at fraction 0. Ties use HitKind, then ID.
std::optional<Hit> FindFirstHit(const glm::vec3& start, const glm::vec3& end,
    std::span<const Collider> colliders);

// Caller supplies the shared gameDt (not raw wall-clock dt).
// Stops/deactivates at the first hit; damage and weapon state belong to caller.
// No work for inactive projectiles, paused time or invalid dt.
std::optional<Hit> AdvanceProjectile(Projectile& projectile, float gameDt,
    std::span<const Collider> colliders);

} // namespace aegis::combat
