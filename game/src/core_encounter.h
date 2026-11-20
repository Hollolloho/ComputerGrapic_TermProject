// 코어실의 수호 드론·포탑과 자기장 공격을 세계 시간으로 갱신한다.
#pragma once
#include "turret_encounter.h"
#include <array>

namespace aegis::combat
{
enum class MagneticPhase { Dormant, Warning, Pulse, Recovery, Defeated };

struct MagneticConfig
{
    float warningSeconds{1.5f};
    float pulseSeconds{1.0f};
    float recoverySeconds{4.0f};
    float radius{6.0f};
    int damage{20};
};

class CoreEncounter
{
public:
    static constexpr ObjectId CoreId = 61000, PlayerId = 61900;
    CoreEncounter();
    void Reset();
    void SetEnvironment(std::span<const Collider> walls, std::span<const Collider> playerWalls);
    bool Fire(const glm::vec3& eye, const glm::vec3& direction);
    bool DropWeapon(const glm::vec3& eye, const glm::vec3& direction);
    bool Interact(const glm::vec3& eye, const glm::vec3& direction);
    WeaponInteraction InteractionHint(const glm::vec3& eye, const glm::vec3& direction) const;
    void Update(float realDeltaTime, float gameDeltaTime, const glm::vec3& eye,
                const glm::vec3& forward = {0, 0, -1});
    bool DamageCore(int damage);

    int Health() const { return health; }
    int CoreHealth() const { return coreHealth; }
    int CoreHits() const { return coreHits; }
    bool CoreAlive() const { return coreHealth > 0; }
    int RemainingEnemies() const;
    EncounterResult Result() const { return result; }
    float DamageFlash() const { return damageFlash; }
    int PlayerShots() const { return playerShots; }
    int EnemyShots() const { return enemyShots; }
    MagneticPhase Phase() const { return phase; }
    float PhaseTime() const { return phaseTime; } // 현재 단계에서 흐른 세계 시간
    float PhaseProgress() const;
    const MagneticConfig& Config() const { return magnetic; }
    const WeaponInventory& Inventory() const { return inventory; }
    const std::array<DroneBehavior, 2>& Drones() const { return drones; }
    const std::array<FixedTurret, 2>& Turrets() const { return turrets; }
    const std::vector<Flight>& PlayerFlights() const { return playerFlights; }
    const std::vector<Flight>& EnemyFlights() const { return enemyFlights; }
    std::vector<Collider> PlayerObstacles() const;
    static glm::vec3 CoreCenter() { return {7.8f, 3.15f, -56.0675f}; }
    static glm::vec3 PlayerRoot() { return {7.8f, 0, -48.2f}; }
    static glm::vec3 SupplyPosition() { return {6.8f, 1.15f, -49.7f}; }
    static Collider CoreTarget();

private:
    std::vector<Collider> Targets() const;
    void UpdatePlayerBullets(float dt);
    void UpdateEnemyBullets(float dt, const glm::vec3& eye, const glm::vec3& forward);
    void UpdateMagnetic(float dt, const glm::vec3& eye);
    void ApplyPulse(const glm::vec3& eye);
    void SpawnEnemyBullet(const glm::vec3& muzzle, const glm::vec3& target, ObjectId source);
    float PhaseDuration() const;
    std::array<DroneBehavior, 2> drones;
    std::array<FixedTurret, 2> turrets;
    std::vector<Collider> walls, movementWalls;
    WeaponInventory inventory;
    std::vector<Flight> playerFlights, enemyFlights;
    MagneticConfig magnetic;
    EncounterResult result{EncounterResult::Running};
    MagneticPhase phase{MagneticPhase::Dormant};
    float phaseTime{}, cooldown{}, damageFlash{};
    bool pulseHit{};
    int health{100}, coreHealth{100}, coreHits{}, playerShots{}, enemyShots{};
};
} // namespace aegis::combat
