#include "turret_encounter.h"

#include <algorithm>
#include <cmath>

namespace aegis::combat {
namespace {
constexpr ObjectId turretId=500;
constexpr ObjectId playerId=600;
const glm::vec3 muzzle{0.0f,1.65f,-9.25f};

bool Finite(const glm::vec3& p)
{
    return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z);
}

void RemoveExpired(std::vector<Flight>& flights)
{
    std::erase_if(flights,[](const Flight& flight) {
        return !flight.projectile.active || flight.age>=4.0f;
    });
}
}

TurretEncounter::TurretEncounter() { Reset(); }

void TurretEncounter::Reset()
{
    colliders={{
        {turretId,HitKind::Body,{-0.6f,0,-10.4f},{0.6f,2.25f,-9.5f}},
        {200,HitKind::Wall,{-0.85f,0,-6.3f},{0.85f,2.5f,-5.8f},false},
        {300,HitKind::Wall,{-6,-0.2f,-14},{6,0,3}},
        {301,HitKind::Wall,{-6,0,-13},{6,4,-12.8f}}
    }};
    playerFlights.clear(); enemyFlights.clear();
    result=EncounterResult::Running; phase=TurretPhase::Idle;
    phaseTime=playerCooldown=damageFlash=0;
    health=100; playerShots=enemyShots=hitsTaken=blockedEnemyShots=0;
    aimPoint=glm::vec3(0);
}

bool TurretEncounter::Fire(const glm::vec3& eye,const glm::vec3& direction)
{
    if (result!=EncounterResult::Running || playerCooldown>0 || playerFlights.size()>=64) return false;
    auto projectile=MakeProjectile(eye,direction,12.0f,7);
    if (!projectile) return false;
    playerFlights.push_back({*projectile,0});
    playerCooldown=0.25f; ++playerShots;
    return true;
}

void TurretEncounter::ToggleCover()
{
    if (result==EncounterResult::Running) colliders[1].enabled=!colliders[1].enabled;
}

void TurretEncounter::Update(float realDt,float gameDt,const glm::vec3& eye)
{
    if (result!=EncounterResult::Running || !std::isfinite(realDt) || !std::isfinite(gameDt)
        || realDt<0 || gameDt<0 || !Finite(eye)) return;
    playerCooldown=std::max(0.0f,playerCooldown-realDt);
    damageFlash=std::max(0.0f,damageFlash-gameDt);
    if (gameDt==0) return;

    // Player shots never collide with the shooter's body. One impact per bullet.
    for (auto& flight:playerFlights) {
        if (auto hit=AdvanceProjectile(flight.projectile,gameDt,colliders)) {
            if (hit->objectId==turretId) {
                colliders[0].enabled=false;
                phase=TurretPhase::Destroyed;
                result=EncounterResult::Cleared;
                break;
            }
        }
        flight.age+=gameDt;
    }
    if (result==EncounterResult::Cleared) {
        playerFlights.clear(); enemyFlights.clear(); return;
    }
    RemoveExpired(playerFlights);

    // Same environment proxies, but enemy shots ignore their source turret.
    std::array<Collider,5> enemyColliders;
    std::copy(colliders.begin(),colliders.end(),enemyColliders.begin());
    enemyColliders[0].enabled=false;
    enemyColliders[4]={playerId,HitKind::Body,eye+glm::vec3(-0.3f,-1.65f,-0.3f),
        eye+glm::vec3(0.3f,0.15f,0.3f)};
    for (auto& flight:enemyFlights) {
        if (auto hit=AdvanceProjectile(flight.projectile,gameDt,enemyColliders)) {
            if (hit->objectId==playerId) {
                health=std::max(0,health-25); ++hitsTaken; damageFlash=0.3f;
            } else ++blockedEnemyShots;
        }
        flight.age+=gameDt;
        if (health==0) break;
    }
    RemoveExpired(enemyFlights);
    if (health==0) {
        result=EncounterResult::Failed;
        playerFlights.clear(); enemyFlights.clear(); return;
    }

    const auto sight=FindFirstHit(muzzle,eye,enemyColliders);
    const bool visible=sight && sight->objectId==playerId;
    if (phase==TurretPhase::Cooldown) {
        phaseTime-=gameDt;
        if (phaseTime<=0) phase=TurretPhase::Idle;
        return;
    }
    if (!visible) {
        phase=TurretPhase::Idle; phaseTime=0; return;
    }
    if (phase==TurretPhase::Idle) {
        phase=TurretPhase::Warning; phaseTime=1.0f; aimPoint=eye;
        return;
    }
    phaseTime-=gameDt;
    if (phaseTime<=0) {
        if (auto projectile=MakeProjectile(muzzle,aimPoint-muzzle,7.0f,900)) {
            enemyFlights.push_back({*projectile,0}); ++enemyShots;
        }
        phase=TurretPhase::Cooldown; phaseTime=1.2f;
    }
}

} // namespace aegis::combat
