#pragma once
#include "combat.h"
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

class FixedTurret
{
public:
    static constexpr ObjectId BodyId = 502, WeaponId = 503, ShotId = 901;
    bool SetPosition(const glm::vec3& point);
    void SetModelSize(float modelHalfWidth, float modelHeight, float modelPivotHeight, float modelBarrelReach);
    void Reset();
    bool Update(float gameDeltaTime, const glm::vec3& eye, std::span<const Collider> walls);
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
    Collider Weapon() const;

private:
    glm::vec3 position{-1.55f, 0, -15.9f}, aimPoint{0}, direction{0, 0, 1};
    TurretPhase phase{TurretPhase::Idle};
    float timer{};
    float halfWidth{0.5f}, height{2.2f}, pivotHeight{1.65f}, barrelReach{0.85f};
};
} // namespace aegis::combat
