#pragma once
#include "gameplay/combat/combat.h"
#include <glm/mat4x4.hpp>

namespace aegis::combat
{
enum class TurretPhase
{
    Idle,
    Warning,
    Cooldown,
    Disarmed,
    Destroyed
};

enum class TurretType { SingleC, Twin };

class FixedTurret
{
public:
    static constexpr ObjectId BodyId = 502, WeaponId = 503, ShotId = 901;
    bool SetPosition(const glm::vec3& point);
    void Configure(TurretType model, const glm::vec3& point, ObjectId index = 0, float rotYDegrees = 0);
    TurretType Type() const { return type; }
    ObjectId BodyTarget() const { return BodyId + 2 * number; }
    ObjectId WeaponTarget() const { return WeaponId + 2 * number; }
    ObjectId ShotSource() const { return ShotId + number; }
    void Reset();
    bool Update(float gameDeltaTime, const glm::vec3& eye, const std::vector<Collider>& walls);
    void Disarm();
    void Destroy();

    bool Alive() const
    {
        return phase != TurretPhase::Destroyed;
    }

    bool Armed() const
    {
        return Alive() && phase != TurretPhase::Disarmed;
    }

    TurretPhase Phase() const
    {
        return phase;
    }

    const glm::vec3& Position() const
    {
        return position;
    }

    const glm::vec3& AimPoint() const
    {
        return aimPoint;
    }

    glm::vec3 Muzzle() const;
    glm::mat4 BarrelTransform() const;
    Collider Body() const;
    Collider Weapon(int barrel = 0) const;

private:
    float BarrelOffset(int barrel) const;
    glm::vec3 position{-1.55f, 0, -15.9f}, aimPoint{0}, direction{0, 0, 1};
    TurretPhase phase{TurretPhase::Idle};
    float timer{};
    TurretType type{TurretType::SingleC};
    ObjectId number{};
    float spawnRotY{};
    int firingBarrel{};
};
} // namespace aegis::combat
