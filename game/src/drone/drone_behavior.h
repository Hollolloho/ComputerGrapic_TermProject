// 드론의 순찰, 경고, 조준과 사격 시점을 계산한다.
#pragma once
#include "gameplay/combat/combat.h"
#include <glm/mat4x4.hpp>

namespace aegis::combat
{
enum class DronePhase
{
    Patrol,
    Tracking,
    Warning,
    Waiting,
    Disarmed,
    Destroyed
};

// 상태 하나와 남은 시간으로 행동을 결정한다. 느린 세계 시간을 전달받는다.
class DroneBehavior
{
public:
    bool SetScale(float value);

    float Scale() const
    {
        return scale;
    }

    void Reset();
    bool SetSpawn(const glm::vec3& point, float rotYRadians, float endZ);
    bool Update(float gameDeltaTime, const glm::vec3& eye, const std::vector<Collider>& walls);
    void Disarm();
    void Destroy();

    DronePhase Phase() const
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

    float RotationYRadians() const
    {
        return rotY;
    } // Radians; model front is +Z.

    glm::mat4 WorldTransform() const;
    glm::vec3 Muzzle() const;
    Collider Body() const;
    Collider Weapon() const;

    bool Armed() const
    {
        return phase != DronePhase::Disarmed && phase != DronePhase::Destroyed;
    }

private:
    Collider Bounds(ObjectId id, HitKind kind, const glm::vec3& low, const glm::vec3& high) const;
    bool Visible(const glm::vec3& eye, const std::vector<Collider>& walls) const;
    bool TurnToward(const glm::vec3& eye, float gameDeltaTime);
    void MovingDrone(float gameDeltaTime, const std::vector<Collider>& walls);

    bool DetectPlayer(const glm::vec3& eye, const std::vector<Collider>& walls) const;

    glm::vec3 spawn{-0.65f, 1.8f, -10};
    float spawnRotY{};
    float patrolEndZ{-10.f};
    float patrolRotY{};
    float scale{1};
    glm::vec3 position{-0.65f, 1.8f, -10};
    glm::vec3 aimPoint{0};
    DronePhase phase{DronePhase::Patrol};
    float rotY{};
    double timer{1.5};
    bool movingToEnd{true};
    bool isTurning{};
    float turnDegrees{};
};
} // namespace aegis::combat
