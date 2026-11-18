#pragma once
#include <array>
#include <glm/glm.hpp>
namespace aegis::combat {
struct EnemyDebris { glm::mat4 world{1}; float age{}; bool active{}; };
// Visual state only. It never owns a gun, collider or damage event.
class DestructionEffects {
public:
    static constexpr float DebrisLifetime=1.4f,DeathDuration=.8f;
    static constexpr int DronePieceCount=4,TurretPieceCount=3;
    void Reset();
    void ObserveEnemy(int slot,bool alive,const glm::mat4& world);
    void UpdateWorld(float gameDt);
    void ObserveHealth(int health);
    void UpdateDeath(float realDt);
    const EnemyDebris& Debris(int slot) const { return debris.at(slot); }
    int Bursts() const { return bursts; }
    int DeathEvents() const { return deaths; }
    bool DeathStarted() const { return deaths!=0; }
    bool DeathPlaying() const { return DeathStarted() && deathAge<DeathDuration; }
    float DeathProgress() const { return deathAge/DeathDuration; }
    static glm::mat4 PieceTransform(const glm::mat4& world,int piece,float age);
private:
    std::array<EnemyDebris,2> debris{};
    std::array<bool,2> known{},wasAlive{};
    int bursts{},deaths{};
    float deathAge{};
};
}
