#pragma once
#include "combat.h"

namespace aegis::combat {
enum class DronePhase { Patrol, Warning, Waiting, Disarmed, Destroyed };

// Deterministic, fixed-route enemy controller. All clocks use game time.
class DroneBehavior {
public:
    void Reset();
    bool SetSpawn(const glm::vec3& point,float yawRadians,float patrolCenterX);
    bool Update(float gameDt,const glm::vec3& eye,std::span<const Collider> walls);
    void Disarm();
    void Destroy();
    DronePhase Phase() const { return phase; }
    const glm::vec3& Position() const { return position; }
    const glm::vec3& AimPoint() const { return aimPoint; }
    float Yaw() const { return yaw; } // Radians; model front is +Z.
    glm::vec3 Muzzle() const;
    Collider Body() const;
    Collider Weapon() const;
    bool Armed() const { return phase!=DronePhase::Disarmed && phase!=DronePhase::Destroyed; }

private:
    Collider Bounds(ObjectId id,HitKind kind,const glm::vec3& low,const glm::vec3& high) const;
    bool Visible(const glm::vec3& eye,std::span<const Collider> walls) const;
    bool Move(float dt,std::span<const Collider> walls);
    glm::vec3 spawn{-0.65f,1.8f,-10};
    float spawnYaw{},patrolCenter{};
    glm::vec3 position{-0.65f,1.8f,-10};
    glm::vec3 aimPoint{0};
    DronePhase phase{DronePhase::Patrol};
    float yaw{};
    float timer{1.5f};
    int destination{1};
};
}
