#pragma once

#include "gameplay/combat/combat_encounter.h"
#include "drone/drone_behavior.h"
#include "fixed_turret.h"
#include <initializer_list>

namespace aegis::combat
{

// A fixed-turret encounter. Input, camera, drawing and global time are external.
class TurretEncounter : public CombatEncounter
{
public:
    TurretEncounter();
    void Reset();
    void SetEnvironment(std::span<const Collider> walls);
    void SetDroneMode(bool enabled);

    bool SetDroneScale(float value)
    {
        if (!drone.SetScale(value))
        {
            return false;
        }
        Reset();
        return true;
    }

    bool SetDroneSpawn(const glm::vec3& point, float rotYRadians, float endZ);
    bool SetSupply(const glm::vec3& point);
    void SetExitDoors(ObjectId left, ObjectId right, float distance);
    void SetTurrets(TurretType type, std::initializer_list<glm::vec3> positions, bool primary = true);
    const std::vector<FixedTurret>& Turrets() const { return turrets; }
    int RemainingEnemies() const;
    int EnemyCount() const { return static_cast<int>(turrets.size()) + (primaryEnabled ? 1 : 0); }

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

    void Update(float realDeltaTime, float gameDeltaTime, const glm::vec3& eye,
                const glm::vec3& forward = glm::vec3(0, 0, -1)) override;
    void ToggleCover();

    TurretPhase Phase() const
    {
        return phase;
    }

    int HitsTaken() const
    {
        return hitsTaken;
    }

    int BlockedEnemyShots() const
    {
        return blockedEnemyShots;
    }

    bool CoverEnabled() const
    {
        return colliders[TestCoverIndex].enabled;
    }

    const std::vector<Collider>& Colliders() const
    {
        return colliders;
    }

private:
    std::span<const Collider> InteractionColliders(std::vector<Collider>& storage) const override;
    // 한 프레임의 전투를 아래 함수들로 나누어 처리한다.
    bool UpdateDrone(float gameDeltaTime, const glm::vec3& eye);
    void SyncTurret(std::size_t index);
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
        FirstTurretIndex
    };

    ObjectId leftDoor{}, rightDoor{};
    float doorDistance{};
    float doorProgress{};
    glm::vec3 supplyPosition = WeaponInventory::SupplyPosition();
    std::vector<FixedTurret> turrets;
    bool primaryEnabled{true};
    DroneBehavior drone;
    bool droneMode{};
    std::vector<Collider> colliders;
    std::vector<Collider> environment;
    glm::vec3 aimPoint{0.0f};
    TurretPhase phase{TurretPhase::Idle};
    float phaseTime{};
    int hitsTaken{};
    int blockedEnemyShots{};
};

} // namespace aegis::combat
