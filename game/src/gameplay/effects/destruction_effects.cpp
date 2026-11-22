// 확정된 파괴·사망을 관찰해 효과를 한 번만 시작한다. 피해나 무기는 생성하지 않는다.
#include "destruction_effects.h"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>

namespace aegis::combat
{
void UpdateShards(std::vector<CrystalShard>& shards, float gameDeltaTime)
{
    if (!std::isfinite(gameDeltaTime) || gameDeltaTime <= 0)
        return;

    const glm::vec3 gravity(0, -3.2f, 0);
    for (std::size_t i = 0; i < shards.size();)
    {
        auto& shard = shards[i];
        shard.age += gameDeltaTime;
        if (shard.age >= shard.lifetime)
        {
            shard = shards.back();
            shards.pop_back();
            continue;
        }
        shard.position += shard.velocity * gameDeltaTime + gravity * (0.5f * gameDeltaTime * gameDeltaTime);
        shard.velocity += gravity * gameDeltaTime;
        ++i;
    }
}

void DestructionEffects::Reset()
{
    debris = {};
    known = {};
    wasAlive = {};
    bursts = deaths = 0;
    deathAge = 0;
}

void DestructionEffects::ObserveEnemy(int slot, bool alive, const glm::mat4& world)
{
    if (slot < 0 || slot >= static_cast<int>(debris.size()))
    {
        return;
    }
    if (known[slot] && wasAlive[slot] && !alive)
    {
        debris[slot] = {world, 0, true};
        ++bursts;
    }
    known[slot] = true;
    wasAlive[slot] = alive;
}

void DestructionEffects::UpdateWorld(float gameDeltaTime)
{
    if (!std::isfinite(gameDeltaTime) || gameDeltaTime <= 0)
    {
        return;
    }
    for (auto& item : debris)
    {
        if (item.active)
        {
            item.age = std::min(DebrisLifetime, item.age + gameDeltaTime);
            item.active = item.age < DebrisLifetime;
        }
    }
}

void DestructionEffects::ObserveHealth(int health)
{
    if (health <= 0 && !DeathStarted())
    {
        deaths = 1;
        deathAge = 0;
    }
}

void DestructionEffects::UpdateDeath(float realDeltaTime)
{
    if (DeathPlaying() && std::isfinite(realDeltaTime) && realDeltaTime > 0)
    {
        deathAge = std::min(DeathDuration, deathAge + realDeltaTime);
    }
}

glm::mat4 DestructionEffects::PieceTransform(const glm::mat4& world, int piece, float age)
{
    if (!std::isfinite(age))
    {
        age = 0;
    }
    const float t = std::clamp(age, 0.0f, DebrisLifetime), angle = piece * 2.39996323f;
    const glm::vec3 velocity(std::cos(angle) * (1.2f + .2f * piece), 1.4f + .25f * (piece % 3),
                             std::sin(angle) * 1.3f);
    const auto offset = velocity * t + glm::vec3(0, -2.2f * t * t, 0);
    const float scale = std::clamp((DebrisLifetime - t) / .4f, 0.0f, 1.0f);
    return glm::scale(glm::rotate(glm::translate(world, offset), t * (2 + piece),
                                  glm::normalize(glm::vec3(1, .4f + piece, .7f))),
                      glm::vec3(scale));
}
} // namespace aegis::combat
