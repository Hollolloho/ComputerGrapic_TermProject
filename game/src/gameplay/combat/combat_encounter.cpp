#include "combat_encounter.h"
#include <algorithm>

namespace aegis::combat
{

void CombatEncounter::ResetCombat(const glm::vec3& supplyPosition)
{
    inventory.Reset();
    inventory.SetSupplyPosition(supplyPosition);
    playerFlights.clear();
    enemyFlights.clear();
    result = EncounterResult::Running;
    health = 100;
    playerShots = enemyShots = 0;
    damageFlash = playerCooldown = 0;
}

void CombatEncounter::UpdateShotCooldown(float realDeltaTime)
{
    playerCooldown = std::max(0.0f, playerCooldown - realDeltaTime);
}

bool CombatEncounter::Fire(const glm::vec3& eye, const glm::vec3& direction)
{
    if (result != EncounterResult::Running || !inventory.Armed() || playerCooldown > 0 ||
        playerFlights.size() >= 64)
    {
        return false;
    }
    const auto shot = MakeBullet(eye, direction, 12.0f, inventory.EquippedId());
    if (!shot)
    {
        return false;
    }
    playerFlights.push_back({*shot, 0});
    playerCooldown = 0.25f;
    ++playerShots;
    return true;
}

bool CombatEncounter::DropWeapon(const glm::vec3& eye, const glm::vec3& direction)
{
    return result != EncounterResult::Failed && inventory.Drop(eye, direction);
}

bool CombatEncounter::Interact(const glm::vec3& eye, const glm::vec3& direction)
{
    std::vector<Collider> storage;
    return result != EncounterResult::Failed &&
           inventory.Interact(eye, direction, InteractionColliders(storage));
}

WeaponInteraction CombatEncounter::InteractionHint(const glm::vec3& eye, const glm::vec3& direction) const
{
    std::vector<Collider> storage;
    return result == EncounterResult::Failed ? WeaponInteraction::None
                                             : inventory.Hint(eye, direction, InteractionColliders(storage));
}

} // namespace aegis::combat
