#pragma once
#include <glm/glm.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <span>

namespace aegis::combat {
struct WeaponShard {
    glm::vec3 position{},velocity{};
    float age{},lifetime{},size{};
};
// Presentation only: observes committed health/break counters; never changes combat.
// The caller supplies gameDt, including zero when the world is stopped.
class CombatFeedback {
public:
    void Reset(int health=100,int breaks=0) {
        lastHealth=health; lastBreaks=breaks; count=0; damage=0; bursts=hits=0;
    }
    void Observe(int health,int breaks,const glm::vec3& origin,
        const glm::vec3& right,const glm::vec3& forward) {
        if(health<lastHealth) { damage=0.28f; ++hits; }
        if(breaks>lastBreaks) {
            // Only the newly committed break creates a burst; polling cannot duplicate it.
            ++bursts; count=shards.size();
            for(std::size_t i=0;i<count;++i) {
                const float angle=static_cast<float>(i)*2.39996323f;
                const float radius=0.025f+static_cast<float>(i)*0.007f;
                const float side=std::cos(angle),lift=std::sin(angle);
                shards[i]={origin+right*(side*radius)+glm::vec3(0,lift*radius*0.6f,0),
                    right*(side*(0.4f+radius*4))+glm::vec3(0,0.4f+lift*0.7f,0)
                    +forward*(0.1f+static_cast<float>((i*7)%5)*0.09f),0,
                    0.65f+static_cast<float>(i%4)*0.08f,0.012f+static_cast<float>((i*7)%5)*0.003f};
            }
        }
        lastHealth=health; lastBreaks=breaks;
    }
    void Update(float gameDt) {
        if(!std::isfinite(gameDt) || gameDt<=0) return;
        damage=std::max(0.0f,damage-gameDt);
        const glm::vec3 gravity(0,-3.2f,0);
        for(std::size_t i=0;i<count;) {
            auto& shard=shards[i];
            if(shard.age+gameDt>=shard.lifetime) { shard=shards[--count]; continue; }
            shard.position+=shard.velocity*gameDt+gravity*(0.5f*gameDt*gameDt);
            shard.velocity+=gravity*gameDt; shard.age+=gameDt; ++i;
        }
    }
    float DamageStrength() const { return damage/0.28f; }
    std::span<const WeaponShard> Shards() const { return {shards.data(),count}; }
    int BreakBursts() const { return bursts; }
    int DamageEvents() const { return hits; }
private:
    std::array<WeaponShard,18> shards{};
    std::size_t count{};
    int lastHealth{100},lastBreaks{},bursts{},hits{};
    float damage{};
};
}
