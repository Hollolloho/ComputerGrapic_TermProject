#pragma once
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

struct EnemyDebris
{
    glm::mat4 world{1};
    float age{};
    bool active{};
};

// Visual state only. It never owns a gun, collider or damage event.
class DestructionEffects
{
public:
    static constexpr float DebrisLifetime = 1.4f, DeathDuration = .8f;
    static constexpr int DronePieceCount = 4;
    void Reset(std::size_t enemyCount = 3);
    void ObserveEnemy(std::size_t slot, bool alive, const glm::mat4& world);
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
