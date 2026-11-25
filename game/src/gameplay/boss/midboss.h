#pragma once

#include "boss_geometry.h"

namespace aegis::combat
{
enum class LaserPhase { Waiting, Charging, Firing, Cooling, Defeated };

struct MidbossBeam
{
    glm::vec3 origin{}, direction{}, right{}, up{};
    float length{}, halfWidth{};
};

class Midboss
{
public:
    void SetMount(const glm::vec3& position, float scale);
    void Clear();
    void Reset();
    int Update(float gameDeltaTime, const glm::vec3& eye, const std::vector<Collider>& walls);

    LaserPhase Phase() const;
    float ChargeProgress() const;
    bool Enabled() const { return enabled; }
    bool Alive() const { return enabled && coreHits < CoreHitLimit; }
    int CoreHits() const { return coreHits; }
    const BossPose& Pose() const { return pose; }
    const MidbossBeam& Beam() const { return beam; }
    bool FindHit(const glm::vec3& start, const glm::vec3& end, Hit& hit) const;
    void HitCore();
    bool CoreExposed() const { return Phase() == LaserPhase::Cooling && !cooldownHit; }

    static const int CoreHitLimit = 3;
    static const int DamagePerTick = 25;
    static const ObjectId CoreId = 54001;

private:
    void UpdatePose();
    void SetBeamDirection(const glm::vec3& direction);
    void AimAt(const glm::vec3& eye, float gameDeltaTime);
    void ClipBeam(const std::vector<Collider>& walls);

    BossPose pose{};
    glm::mat4 root{1};
    MidbossBeam beam;
    glm::vec3 previousEye{};
    double cycleTime{};
    int coreHits{};
    bool enabled{}, cooldownHit{}, hasPreviousEye{};
};
} // namespace aegis::combat
