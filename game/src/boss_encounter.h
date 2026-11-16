#pragma once
#include "boss_geometry.h"
#include "turret_encounter.h"

namespace aegis::combat {
enum class BossPhase { Warning, Attack, Opening, Exposed, Closing, Recovery, Defeated };
class BossEncounter {
public:
    BossEncounter();
    void Reset();
    void Update(float realDt,float gameDt,const glm::vec3& eye,const glm::vec3& forward={0,0,-1});
    bool Fire(const glm::vec3& eye,const glm::vec3& direction);
    bool DropWeapon(const glm::vec3& eye,const glm::vec3& direction);
    bool Interact(const glm::vec3& eye,const glm::vec3& direction);
    WeaponInteraction InteractionHint(const glm::vec3& eye,const glm::vec3& direction) const;
    EncounterResult Result() const { return result; }
    BossPhase Phase() const { return phase; }
    const BossPose& Pose() const { return pose; }
    float ArmorOpen() const { return armorOpen; }
    glm::vec3 CoreCenter() const { return BossCoreCenter(pose); }
    int CoreHits() const { return coreHits; }
    int Health() const { return health; }
    int PlayerShots() const { return playerShots; }
    int EnemyShots() const { return enemyShots; }
    int BlockedShots() const { return blockedShots; }
    float DamageFlash() const { return damageFlash; }
    const WeaponInventory& Inventory() const { return inventory; }
    const std::vector<Flight>& PlayerFlights() const { return playerFlights; }
    const std::vector<Flight>& EnemyFlights() const { return enemyFlights; }
    static glm::vec3 FiringRoot() { return {0,0,-2.4f}; }
    static glm::vec3 CoverRoot() { return {2.72f,0,-4.05f}; }
private:
    void AdvancePattern(float dt,const glm::vec3& eye);
    void Shoot(const glm::vec3& eye);
    WeaponInventory inventory;
    BossPose pose;
    std::vector<Flight> playerFlights,enemyFlights;
    EncounterResult result{EncounterResult::Running};
    BossPhase phase{BossPhase::Warning};
    glm::vec3 aimPoint{0,1.05f,-2.4f};
    float timer{1.2f},armorOpen{},cooldown{},damageFlash{};
    int health{100},coreHits{},playerShots{},enemyShots{},blockedShots{};
};
}
