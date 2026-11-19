#pragma once

#include "combat_range.h"
#include "weapon_inventory.h"
#include "drone_behavior.h"
#include "fixed_turret.h"

namespace aegis::combat
{

enum class EncounterResult
{
    Running,
    Securing,
    Failed,
    Cleared
};

// A fixed-turret encounter. Input, camera, drawing and global time are external.
class TurretEncounter
{
public:
    TurretEncounter();
    void Reset();
    void SetEnvironment(std::span<const Collider> walls);
    void SetDroneMode(bool enabled);
    bool SetDroneSpawn(const glm::vec3& point, float yawRadians, float patrolCenterX);
    bool SetSupply(const glm::vec3& point);
    void SetExitDoors(ObjectId left, ObjectId right, float distance);
    void SetSupportTurret(bool enabled);

    bool HasSupportTurret() const
    {
        return supportEnabled;
    }

    const FixedTurret& SupportTurret() const
    {
        return support;
    }

    int RemainingEnemies() const
    {
        return (EnemyAlive() ? 1 : 0) + (supportEnabled && support.Alive() ? 1 : 0);
    }

    bool CanAnalyze() const
    {
        return result == EncounterResult::Cleared;
    }

    float DoorProgress() const
    {
        return doorProgress;
    }

    bool EnemyAlive() const
    {
        return colliders[PrimaryBodyIndex].enabled;
    }

    bool IsDroneMode() const
    {
        return droneMode;
    }

    const DroneBehavior& DroneState() const
    {
        return drone;
    }

    bool Fire(const glm::vec3& eye, const glm::vec3& direction);
    void Update(float realDeltaTime, float gameDeltaTime, const glm::vec3& eye,
                const glm::vec3& forward = glm::vec3(0, 0, -1));
    void ToggleCover();

    const WeaponInventory& Inventory() const
    {
        return inventory;
    }

    bool DropWeapon(const glm::vec3& eye, const glm::vec3& direction);
    bool Interact(const glm::vec3& eye, const glm::vec3& direction);
    WeaponInteraction InteractionHint(const glm::vec3& eye, const glm::vec3& direction) const;

    EncounterResult Result() const
    {
        return result;
    }

    TurretPhase Phase() const
    {
        return phase;
    }

    int Health() const
    {
        return health;
    }

    int PlayerShots() const
    {
        return playerShots;
    }

    int EnemyShots() const
    {
        return enemyShots;
    }

    int HitsTaken() const
    {
        return hitsTaken;
    }

    int BlockedEnemyShots() const
    {
        return blockedEnemyShots;
    }

    float DamageFlash() const
    {
        return damageFlash;
    }

    bool CoverEnabled() const
    {
        return colliders[TestCoverIndex].enabled;
    }

    const std::vector<Collider>& Colliders() const
    {
        return colliders;
    }

    const std::vector<Flight>& PlayerFlights() const
    {
        return playerFlights;
    }

    const std::vector<Flight>& EnemyFlights() const
    {
        return enemyFlights;
    }

private:
    // 한 프레임의 전투를 아래 함수들로 나누어 처리한다.
    bool UpdateDrone(float gameDeltaTime, const glm::vec3& eye);
    bool UpdateSupportTurret(float gameDeltaTime, const glm::vec3& eye);
    void UpdatePlayerBullets(float gameDeltaTime);
    void UpdateEnemyBullets(float gameDeltaTime, const glm::vec3& eye, const glm::vec3& forward);
    void UpdatePrimaryTurret(float gameDeltaTime, const glm::vec3& eye);
    std::vector<Collider> CreateEnemyTargets(const glm::vec3& eye, const glm::vec3& forward) const;
    void SpawnEnemyBullet(const glm::vec3& origin, const glm::vec3& target, ObjectId sourceWeapon);
    void FailEncounter();
    void UpdateDoors(float gameDeltaTime);

    enum ColliderIndex
    {
        PrimaryBodyIndex = 0,
        TestCoverIndex,
        TestFloorIndex,
        TestBackWallIndex,
        PrimaryWeaponIndex,
        SupportBodyIndex,
        SupportWeaponIndex
    };

    ObjectId leftDoor{}, rightDoor{};
    float doorDistance{};
    float doorProgress{};
    glm::vec3 supplyPosition = WeaponInventory::SupplyPosition();
    FixedTurret support;
    bool supportEnabled{};
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
