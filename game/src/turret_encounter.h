#pragma once

#include "combat_range.h"

namespace aegis::combat {

enum class EncounterResult { Running, Failed, Cleared };
enum class TurretPhase { Idle, Warning, Cooldown, Destroyed };

// A fixed-turret encounter. Input, camera, drawing and global time are external.
class TurretEncounter {
public:
    TurretEncounter();
    void Reset();
    bool Fire(const glm::vec3& eye, const glm::vec3& direction);
    void Update(float realDt, float gameDt, const glm::vec3& eye);
    void ToggleCover();
    EncounterResult Result() const { return result; }
    TurretPhase Phase() const { return phase; }
    int Health() const { return health; }
    int PlayerShots() const { return playerShots; }
    int EnemyShots() const { return enemyShots; }
    int HitsTaken() const { return hitsTaken; }
    int BlockedEnemyShots() const { return blockedEnemyShots; }
    float DamageFlash() const { return damageFlash; }
    bool CoverEnabled() const { return colliders[1].enabled; }
    const std::array<Collider,4>& Colliders() const { return colliders; }
    const std::vector<Flight>& PlayerFlights() const { return playerFlights; }
    const std::vector<Flight>& EnemyFlights() const { return enemyFlights; }

private:
    std::array<Collider,4> colliders{};
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
