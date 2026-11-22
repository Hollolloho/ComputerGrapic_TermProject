#include "combat_encounter.h"
#include <algorithm>
#include <cmath>

namespace aegis::combat
{

void CombatEncounter::ResetCombat(const glm::vec3& supplyPosition)
{
    inventory.Reset();
    inventory.SetSupplyPosition(supplyPosition);
    playerFlights.clear();
    enemyFlights.clear();
    impactShards.clear();
    result = EncounterResult::Running;
    health = 100;
    playerShots = enemyShots = 0;
    damageFlash = playerCooldown = 0;
}

void CombatEncounter::UpdateShotCooldown(float realDeltaTime)
{
    playerCooldown = std::max(0.0f, playerCooldown - realDeltaTime);
}

void CombatEncounter::ShatterBullet(const Bullet& bullet, bool enemy)
{
    // 충돌한 지점에서 탄이 날아온 쪽으로 여섯 조각이 퍼진다.
    glm::vec3 back(0, 0, 1);
    const float speed = glm::length(bullet.velocity);
    if (std::isfinite(speed) && speed > 0)
        back = -bullet.velocity / speed;
    glm::vec3 up(0, 1, 0);
    if (std::abs(back.y) > 0.95f)
        up = glm::vec3(1, 0, 0);
    const glm::vec3 right = glm::normalize(glm::cross(up, back));
    up = glm::cross(back, right);

    const int pieceCount = 6;
    const glm::vec3 scatter[pieceCount] = {
        {-0.9f, 0.6f, 0.8f}, {0.7f, 0.8f, 1.1f}, {-0.4f, -0.8f, 0.9f},
        {0.9f, -0.5f, 0.7f}, {0.1f, 1.2f, 0.6f}, {-1.1f, -0.2f, 1.0f}
    };
    for (int i = 0; i < pieceCount; ++i)
    {
        const glm::vec3& piece = scatter[i];
        const glm::vec3 velocity = piece.x * right + piece.y * up + piece.z * back;
        impactShards.push_back({bullet.position, velocity, 0, 0.36f + 0.025f * i,
                                0.022f + 0.005f * (i % 3), enemy});
    }
}

bool CombatEncounter::Fire(const glm::vec3& eye, const glm::vec3& direction)
{
    if (result != EncounterResult::Running || !inventory.Armed() || playerCooldown > 0 ||
        playerFlights.size() >= 64)
    {
        return false;
    }
    Bullet shot;
    if (!MakeBullet(eye, direction, 12.0f, inventory.EquippedId(), shot))
    {
        return false;
    }
    playerFlights.push_back({shot, 0});
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
