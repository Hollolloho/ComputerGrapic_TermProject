#pragma once
#include "gameplay/effects/destruction_effects.h"
#include <glm/glm.hpp>
#include <algorithm>
#include <cmath>
#include <vector>

namespace aegis::combat
{
// Presentation only: observes committed health/break counters; never changes combat.
// The caller supplies gameDeltaTime, including zero when the world is stopped.
class CombatFeedback
{
public:
    void Reset(int health = 100, int breaks = 0)
    {
        lastHealth = health;
        lastBreaks = breaks;
        shards.clear();
        damage = 0;
        bursts = hits = 0;
    }

    void Observe(int health, int breaks, const glm::vec3& origin, const glm::vec3& right,
                 const glm::vec3& forward)
    {
        if (health < lastHealth)
        {
            damage = 0.28f;
            ++hits;
        }
        if (breaks > lastBreaks)
        {
            // Only the newly committed break creates a burst; polling cannot duplicate it.
            ++bursts;
            shards.resize(18);
            for (std::size_t i = 0; i < shards.size(); ++i)
            {
                const float angle = static_cast<float>(i) * 2.39996323f;
                const float radius = 0.025f + static_cast<float>(i) * 0.007f;
                const float side = std::cos(angle), lift = std::sin(angle);
                shards[i] = {origin + right * (side * radius) + glm::vec3(0, lift * radius * 0.6f, 0),
                             right * (side * (0.4f + radius * 4)) + glm::vec3(0, 0.4f + lift * 0.7f, 0) +
                                 forward * (0.1f + static_cast<float>((i * 7) % 5) * 0.09f),
                             0, 0.65f + static_cast<float>(i % 4) * 0.08f,
                             0.012f + static_cast<float>((i * 7) % 5) * 0.003f};
            }
        }
        lastHealth = health;
        lastBreaks = breaks;
    }

    void Update(float gameDeltaTime)
    {
        if (!std::isfinite(gameDeltaTime) || gameDeltaTime <= 0)
        {
            return;
        }
        damage = std::max(0.0f, damage - gameDeltaTime);
        UpdateShards(shards, gameDeltaTime);
    }

    float DamageStrength() const
    {
        return damage / 0.28f;
    }

    const std::vector<CrystalShard>& Shards() const
    {
        return shards;
    }

    int BreakBursts() const
    {
        return bursts;
    }

    int DamageEvents() const
    {
        return hits;
    }

private:
    std::vector<CrystalShard> shards;
    int lastHealth{100};
    int lastBreaks{};
    int bursts{};
    int hits{};
    float damage{};
};
} // namespace aegis::combat
