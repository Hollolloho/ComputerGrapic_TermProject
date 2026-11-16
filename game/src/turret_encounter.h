#pragma once

#include "combat_range.h"
#include "weapon_inventory.h"
#include "drone_behavior.h"

namespace aegis::combat {

enum class EncounterResult { Running, Securing, Failed, Cleared };
enum class TurretPhase { Idle, Warning, Cooldown, Disarmed, Destroyed };

// A fixed-turret encounter. Input, camera, drawing and global time are external.
class TurretEncounter {
public:
    TurretEncounter();
    void Reset();
    void SetEnvironment(std::span<const Collider> walls);
    void SetDroneMode(bool enabled);
    void SetExitDoors(ObjectId left,ObjectId right,float distance);
    bool CanAnalyze() const { return result==EncounterResult::Cleared; }
    float DoorProgress() const { return doorProgress; }
    bool EnemyAlive() const { return colliders[0].enabled; }
    bool IsDroneMode() const { return droneMode; }
    const DroneBehavior& DroneState() const { return drone; }
    bool Fire(const glm::vec3& eye, const glm::vec3& direction);
    void Update(float realDt, float gameDt, const glm::vec3& eye,
        const glm::vec3& forward=glm::vec3(0,0,-1));
    void ToggleCover();
    const WeaponInventory& Inventory() const { return inventory; }
    bool DropWeapon(const glm::vec3& eye,const glm::vec3& direction);
    bool Interact(const glm::vec3& eye,const glm::vec3& direction);
    WeaponInteraction InteractionHint(const glm::vec3& eye,const glm::vec3& direction) const;

    EncounterResult Result() const { return result; }
    TurretPhase Phase() const { return phase; }
    int Health() const { return health; }
    int PlayerShots() const { return playerShots; }
    int EnemyShots() const { return enemyShots; }
    int HitsTaken() const { return hitsTaken; }
    int BlockedEnemyShots() const { return blockedEnemyShots; }
    float DamageFlash() const { return damageFlash; }
    bool CoverEnabled() const { return colliders[1].enabled; }
    const std::vector<Collider>& Colliders() const { return colliders; }
    const std::vector<Flight>& PlayerFlights() const { return playerFlights; }
    const std::vector<Flight>& EnemyFlights() const { return enemyFlights; }

private:
    void UpdateDoors(float gameDt);
    ObjectId leftDoor{},rightDoor{};
    float doorDistance{},doorProgress{};
    DroneBehavior drone;
    bool droneMode{};
    WeaponInventory inventory;
    std::vector<Collider> colliders;
    std::vector<Collider> environment;
    std::vector<Flight> playerFlights;
    std::vector<Flight> enemyFlights;
    glm::vec3 aimPoint{0.0f};
    EncounterResult result{EncounterResult::Running};
    TurretPhase phase{TurretPhase::Idle};
    float phaseTime{};
    float playerCooldown{};
    float damageFlash{};
    int health{100};
    int playerShots{};
    int enemyShots{};
    int hitsTaken{};
    int blockedEnemyShots{};
};

} // namespace aegis::combat
