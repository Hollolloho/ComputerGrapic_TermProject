#pragma once
#include "drone/drone_debris.generated.h"
#include <cstddef>
#include <vector>
#include <glm/glm.hpp>

namespace aegis::combat
{
struct CrystalShard
{
    glm::vec3 position{}, velocity{};
    float age{};
    float lifetime{};
    float size{};
    bool enemy{};
};

void UpdateShards(std::vector<CrystalShard>& shards, float gameDeltaTime);

struct DroneFragment
{
    glm::vec3 position{}, velocity{};
    glm::mat4 rotation{1};
    float spin{}, scale{1};
};

struct EnemyDebris
{
    glm::mat4 world{1};
    float age{};
    bool active{};
    bool drone{};
    std::array<DroneFragment, drone_debris_asset::pieceCount> pieces{};
};

// Visual state only. It never owns a gun, collider or damage event.
class DestructionEffects
{
public:
    static constexpr float DebrisLifetime = 1.4f, DeathDuration = .8f;
    static constexpr float DroneDebrisLifetime = 6.f;
    static constexpr int DronePieceCount = static_cast<int>(drone_debris_asset::pieceCount);
    void Reset(std::size_t enemyCount = 3);
    void ObserveEnemy(std::size_t slot, bool alive, const glm::mat4& world);
    void ObserveDrone(std::size_t slot, bool alive, const glm::mat4& world, const glm::vec3& direction);
    void UpdateWorld(float gameDeltaTime);
    void ObserveHealth(int health);
    void UpdateDeath(float realDeltaTime);

    const EnemyDebris& Debris(std::size_t slot) const
    {
        return enemies.at(slot).debris;
    }

    int Bursts() const
    {
        return bursts;
    }

    int DeathEvents() const
    {
        return deaths;
    }

    bool DeathStarted() const
    {
        return deaths != 0;
    }

    bool DeathPlaying() const
    {
        return DeathStarted() && deathAge < DeathDuration;
    }

    float DeathProgress() const
    {
        return deathAge / DeathDuration;
    }

    static glm::mat4 DronePieceTransform(const EnemyDebris& debris, std::size_t piece);
    static glm::mat4 PieceTransform(const glm::mat4& world, int piece, float age);

private:
    struct EnemyState
    {
        EnemyDebris debris;
        bool known{};
        bool wasAlive{};
    };
    std::vector<EnemyState> enemies = std::vector<EnemyState>(3);
    int bursts{};
    int deaths{};
    float deathAge{};
};
} // namespace aegis::combat
