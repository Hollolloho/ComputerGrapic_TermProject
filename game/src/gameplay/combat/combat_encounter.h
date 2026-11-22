#pragma once

#include "combat.h"
#include "gameplay/effects/destruction_effects.h"
#include "gameplay/weapons/weapon_inventory.h"

namespace aegis::combat
{

enum class EncounterResult
{
    Running,
    Securing,
    Failed,
    Cleared
};

class CombatEncounter
{
public:
    virtual ~CombatEncounter() = default;
    virtual void Update(float realDeltaTime, float gameDeltaTime, const glm::vec3& eye,
                        const glm::vec3& forward) = 0;

    bool Fire(const glm::vec3& eye, const glm::vec3& direction);
    bool DropWeapon(const glm::vec3& eye, const glm::vec3& direction);
    bool Interact(const glm::vec3& eye, const glm::vec3& direction);
    WeaponInteraction InteractionHint(const glm::vec3& eye, const glm::vec3& direction) const;

    EncounterResult Result() const { return result; }
    int Health() const { return health; }
    int PlayerShots() const { return playerShots; }
    int EnemyShots() const { return enemyShots; }
    float DamageFlash() const { return damageFlash; }
    const WeaponInventory& Inventory() const { return inventory; }
    const std::vector<Flight>& PlayerFlights() const { return playerFlights; }
    const std::vector<Flight>& EnemyFlights() const { return enemyFlights; }
    const std::vector<CrystalShard>& ImpactShards() const { return impactShards; }

protected:
    void ResetCombat(const glm::vec3& supplyPosition);
    void UpdateShotCooldown(float realDeltaTime);
    void ShatterBullet(const Bullet& bullet, bool enemy);

    WeaponInventory inventory;
    std::vector<Flight> playerFlights, enemyFlights;
    std::vector<CrystalShard> impactShards;
    EncounterResult result{EncounterResult::Running};
    int health{100};
    int enemyShots{};
    float damageFlash{};

private:
    // 매 프레임 충돌체를 만드는 전투에서만 storage를 채운다.
    virtual const std::vector<Collider>& InteractionColliders(std::vector<Collider>& storage) const = 0;

    float playerCooldown{};
    int playerShots{};
};

} // namespace aegis::combat
