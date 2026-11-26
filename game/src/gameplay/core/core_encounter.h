// 코어실의 수호 드론·포탑과 자기장 공격을 세계 시간으로 갱신한다.
#pragma once
#include "gameplay/combat/combat_encounter.h"
#include "combat_map/combat_map_layout.generated.h"
#include "drone/drone_behavior.h"
#include "gameplay/turret/fixed_turret.h"
#include <array>

namespace aegis::combat
{
enum class MagneticPhase { Dormant, Warning, Pulse, Recovery, Defeated };

struct MagneticConfig
{
    float warningSeconds{1.5f};
    float pulseSeconds{1.0f};
    float recoverySeconds{4.0f};
    float radius{combat_map_asset::coreRoomRadius}; // 원형 코어실 전체를 덮는다.
    int damage{20};
};

class CoreEncounter : public CombatEncounter
{
public:
    static constexpr ObjectId CoreId = 61000, PlayerId = 61900;
    static constexpr std::size_t DroneCount = 6, TurretCount = 4;
    CoreEncounter();
    void Reset();
    void SetEnvironment(const std::vector<Collider>& walls, const std::vector<Collider>& playerWalls);
    void Update(float realDeltaTime, float gameDeltaTime, const glm::vec3& eye,
                const glm::vec3& forward = {0, 0, -1}) override;
    bool DamageCore(int damage);

    int CoreHealth() const { return coreHealth; }
    int CoreHits() const { return coreHits; }
    bool CoreAlive() const { return coreHealth > 0; }
    int RemainingEnemies() const;
    MagneticPhase Phase() const { return phase; }
    float PhaseTime() const { return phaseTime; } // 현재 단계에서 흐른 세계 시간
    float PhaseProgress() const;
    const MagneticConfig& Config() const { return magnetic; }
    const std::array<DroneBehavior, DroneCount>& Drones() const { return drones; }
    const std::array<FixedTurret, TurretCount>& Turrets() const { return turrets; }
    std::vector<Collider> PlayerObstacles() const;
    static glm::vec3 CoreCenter() { return combat_map_asset::corePosition + glm::vec3(0, 3.15f, 0); }
    static glm::vec3 PlayerRoot() { return {7.8f, 0, -48.2f}; }
    static glm::vec3 SupplyPosition() { return {6.8f, 1.15f, -49.7f}; }
    static Collider CoreTarget();

private:
    const std::vector<Collider>& InteractionColliders(std::vector<Collider>& storage) const override;
    std::vector<Collider> Targets() const;
    void UpdatePlayerBullets(float dt);
    void UpdateEnemyBullets(float dt, const glm::vec3& eye, const glm::vec3& forward);
    void UpdateMagnetic(float dt, const glm::vec3& eye);
    void ApplyPulse(const glm::vec3& eye, float waveRadius);
    void SpawnEnemyBullet(const glm::vec3& muzzle, const glm::vec3& target, ObjectId source, BulletStyle bulletStyle);
    float PhaseDuration() const;
    std::array<DroneBehavior, DroneCount> drones;
    std::array<FixedTurret, TurretCount> turrets;
    std::vector<Collider> walls, movementWalls;
    MagneticConfig magnetic;
    MagneticPhase phase{MagneticPhase::Dormant};
    float phaseTime{};
    bool pulseChecked{};
    int coreHealth{100}, coreHits{};
};
} // namespace aegis::combat
