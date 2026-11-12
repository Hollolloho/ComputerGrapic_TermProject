#include "combat.h"

#include <algorithm>
#include <cmath>
#include <glm/geometric.hpp>

namespace aegis::combat {
namespace {

bool IsFinite(const glm::vec3& value)
{
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

std::optional<float> IntersectBox(const glm::vec3& start,
    const glm::vec3& end, const Collider& box)
{
    if (!box.enabled || !IsFinite(box.minimum) || !IsFinite(box.maximum)) {
        return std::nullopt;
    }
    // Double intermediates avoid overflow/subtraction loss for float endpoints.
    double enter = 0.0;
    double leave = 1.0;
    for (int axis = 0; axis < 3; ++axis) {
        if (box.minimum[axis] > box.maximum[axis]) {
            return std::nullopt;
        }
        const double delta = static_cast<double>(end[axis]) - start[axis];
        if (delta == 0.0) {
            if (start[axis] < box.minimum[axis] || start[axis] > box.maximum[axis]) {
                return std::nullopt;
            }
            continue;
        }
        double near = (static_cast<double>(box.minimum[axis]) - start[axis]) / delta;
        double far = (static_cast<double>(box.maximum[axis]) - start[axis]) / delta;
        if (near > far) {
            std::swap(near, far);
        }
        enter = std::max(enter, near);
        leave = std::min(leave, far);
        if (enter > leave) {
            return std::nullopt;
        }
    }
    return static_cast<float>(enter);
}

} // namespace

std::optional<Projectile> MakeProjectile(const glm::vec3& origin,
    const glm::vec3& direction, float speed, ObjectId sourceWeaponId)
{
    if (!IsFinite(origin) || !IsFinite(direction) || !std::isfinite(speed) || speed <= 0) {
        return std::nullopt;
    }
    const double length = std::sqrt(static_cast<double>(direction.x) * direction.x
        + static_cast<double>(direction.y) * direction.y
        + static_cast<double>(direction.z) * direction.z);
    if (length == 0.0) {
        return std::nullopt;
    }
    const glm::vec3 velocity{
        static_cast<float>(direction.x / length * speed),
        static_cast<float>(direction.y / length * speed),
        static_cast<float>(direction.z / length * speed)};
    return Projectile{origin, velocity, sourceWeaponId, true};
}

std::optional<Hit> FindFirstHit(const glm::vec3& start, const glm::vec3& end,
    std::span<const Collider> colliders)
{
    if (!IsFinite(start) || !IsFinite(end)) {
        return std::nullopt;
    }
    std::optional<Hit> first;
    for (const auto& box : colliders) {
        const auto fraction = IntersectBox(start, end, box);
        if (!fraction) {
            continue;
        }
        const bool tieWins = first && *fraction == first->fraction
            && (box.kind < first->kind
                || (box.kind == first->kind && box.id < first->objectId));
        if (!first || *fraction < first->fraction || tieWins) {
            glm::vec3 point;
            for (int axis = 0; axis < 3; ++axis) {
                point[axis] = static_cast<float>(static_cast<double>(start[axis])
                    + (static_cast<double>(end[axis]) - start[axis]) * *fraction);
            }
            first = Hit{box.id, box.kind, *fraction, point};
        }
    }
    return first;
}

std::optional<Hit> AdvanceProjectile(Projectile& projectile, float gameDt,
    std::span<const Collider> colliders)
{
    if (!projectile.active || !std::isfinite(gameDt) || gameDt <= 0
        || !IsFinite(projectile.position) || !IsFinite(projectile.velocity)) {
        return std::nullopt;
    }
    const glm::vec3 next = projectile.position + projectile.velocity * gameDt;
    if (!IsFinite(next)) {
        return std::nullopt;
    }
    const auto hit = FindFirstHit(projectile.position, next, colliders);
    projectile.position = hit ? hit->position : next;
    if (hit) {
        projectile.active = false;
    }
    return hit;
}

} // namespace aegis::combat
