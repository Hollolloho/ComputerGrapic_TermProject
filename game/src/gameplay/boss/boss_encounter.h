#pragma once
#include "boss_geometry.h"
#include "gameplay/combat/combat_encounter.h"

namespace aegis::combat
{
enum class BossPhase
{
    Warning,
    Attack,
    Opening,
    Exposed,
    Closing,
    Recovery,
    Defeated
};

class BossEncounter : public CombatEncounter
{
public:
    BossEncounter();
    void Reset();
    void Update(float realDeltaTime, float gameDeltaTime, const glm::vec3& eye,
                const glm::vec3& forward = {0, 0, -1}) override;

    BossPhase Phase() const
    {
        return phase;
    }

    const BossPose& Pose() const
    {
        return pose;
    }

    float ArmorOpen() const
    {
        return armorOpen;
    }

    glm::vec3 CoreCenter() const
    {
        return BossCoreCenter(pose);
    }

    int CoreHits() const
    {
        return coreHits;
    }

    int BlockedShots() const
    {
        return blockedShots;
    }

    static glm::vec3 FiringRoot()
    {
        return {0, 0, -2.4f};
    }

    static glm::vec3 CoverRoot()
    {
        return {2.72f, 0, -4.05f};
    }

private:
    const std::vector<Collider>& InteractionColliders(std::vector<Collider>& storage) const override;
    void UpdatePlayerBullets(float gameDeltaTime);
    void UpdateEnemyBullets(float gameDeltaTime, const glm::vec3& eye, const glm::vec3& forward);
    void AdvancePattern(float dt, const glm::vec3& eye);
    void Shoot(const glm::vec3& eye);
    std::vector<Collider> walls;
    BossPose pose;
    BossPhase phase{BossPhase::Warning};
    glm::vec3 aimPoint{0, 1.05f, -2.4f};
    float timer{1.2f};
    float armorOpen{};
    int coreHits{};
    int blockedShots{};
};
} // namespace aegis::combat
