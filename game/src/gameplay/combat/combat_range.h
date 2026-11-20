#pragma once

#include "combat.h"
#include "gameplay/weapons/weapon_inventory.h"
#include <array>
#include <vector>

namespace aegis::combat
{

// Small deterministic firing range, independent of OpenGL/input/camera.
class CombatRange
{
public:
    CombatRange();
    void Reset();
    bool Fire(const glm::vec3& eye, const glm::vec3& direction);
    void Update(float realDeltaTime, float gameDeltaTime);
    void ToggleCover();

    const WeaponInventory& Inventory() const
    {
        return inventory;
    }

    bool DropWeapon(const glm::vec3& eye, const glm::vec3& direction);
    bool Interact(const glm::vec3& eye, const glm::vec3& direction);
    WeaponInteraction InteractionHint(const glm::vec3& eye, const glm::vec3& direction) const;

    const std::array<Collider, 6>& Colliders() const
    {
        return colliders;
    }

    const std::vector<Flight>& Flights() const
    {
        return flights;
    }

    int RemainingTargets() const;

    int Shots() const
    {
        return shots;
    }

    int BlockedShots() const
    {
        return blockedShots;
    }

    bool CoverEnabled() const
    {
        return colliders[3].enabled;
    }

    const std::optional<Hit>& LastHit() const
    {
        return lastHit;
    }

    float ImpactTime() const
    {
        return impactTime;
    }

private:
    WeaponInventory inventory;
    std::array<Collider, 6> colliders{};
    std::vector<Flight> flights;
    std::optional<Hit> lastHit;
    float cooldown{};
    float impactTime{};
    int shots{};
    int blockedShots{};
};

} // namespace aegis::combat
