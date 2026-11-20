// 개발용 사격장의 표적·탄환·무기를 관리한다.
#include "combat_range.h"

#include <algorithm>
#include <cmath>

namespace aegis::combat
{

CombatRange::CombatRange()
{
    Reset();
}

void CombatRange::Reset()
{
    colliders = {{{100, HitKind::Body, {-2.6f, 1.1f, -10.2f}, {-1.6f, 2.1f, -9.5f}},
                  {101, HitKind::Body, {-0.5f, 1.1f, -10.2f}, {0.5f, 2.1f, -9.5f}},
                  {102, HitKind::Body, {1.6f, 1.1f, -10.2f}, {2.6f, 2.1f, -9.5f}},
                  {200, HitKind::Wall, {-0.85f, 0, -6.3f}, {0.85f, 2.5f, -5.8f}},
                  {300, HitKind::Wall, {-6, -0.2f, -14}, {6, 0, 3}},
                  {301, HitKind::Wall, {-6, 0, -13}, {6, 4, -12.8f}}}};
    inventory.Reset();
    flights.clear();
    lastHit.reset();
    cooldown = impactTime = 0;
    shots = blockedShots = 0;
}

bool CombatRange::Fire(const glm::vec3& eye, const glm::vec3& direction)
{
    if (!inventory.Armed() || cooldown > 0 || flights.size() >= 64)
    {
        return false;
    }
    auto projectile = MakeProjectile(eye, direction, 12.0f, inventory.EquippedId());
    if (!projectile)
    {
        return false;
    }
    flights.push_back({*projectile, 0});
    cooldown = 0.25f;
    ++shots;
    return true;
}

void CombatRange::Update(float realDeltaTime, float gameDeltaTime)
{
    if (!std::isfinite(realDeltaTime) || !std::isfinite(gameDeltaTime) || realDeltaTime < 0 ||
        gameDeltaTime < 0)
    {
        return;
    }
    cooldown = std::max(0.0f, cooldown - realDeltaTime);
    impactTime = std::max(0.0f, impactTime - gameDeltaTime);
    for (auto& flight : flights)
    {
        if (auto hit = AdvanceProjectile(flight.projectile, gameDeltaTime, colliders))
        {
            lastHit = hit;
            impactTime = 0.4f;
            if (hit->kind == HitKind::Body)
            {
                for (auto& collider : colliders)
                {
                    if (collider.id == hit->objectId)
                    {
                        collider.enabled = false;
                        const auto centre = (collider.minimum + collider.maximum) * 0.5f;
                        inventory.SpawnDrop({centre.x, 0.12f, centre.z});
                    }
                }
                inventory.Break(flight.projectile.sourceWeaponId);
            }
            else
            {
                ++blockedShots;
            }
        }
        flight.age += gameDeltaTime;
    }
    std::erase_if(flights,
                  [](const Flight& flight) { return !flight.projectile.active || flight.age >= 4.0f; });
}

bool CombatRange::DropWeapon(const glm::vec3& eye, const glm::vec3& direction)
{
    return inventory.Drop(eye, direction);
}

bool CombatRange::Interact(const glm::vec3& eye, const glm::vec3& direction)
{
    return inventory.Interact(eye, direction, colliders);
}

WeaponInteraction CombatRange::InteractionHint(const glm::vec3& eye, const glm::vec3& direction) const
{
    return inventory.Hint(eye, direction, colliders);
}

void CombatRange::ToggleCover()
{
    colliders[3].enabled = !colliders[3].enabled;
}

int CombatRange::RemainingTargets() const
{
    return static_cast<int>(std::count_if(colliders.begin(), colliders.end(), [](const Collider& box)
                                          { return box.kind == HitKind::Body && box.enabled; }));
}

} // namespace aegis::combat
