#include "turret_encounter.h"

#include <algorithm>
#include <cmath>

namespace aegis::combat {
namespace {
constexpr ObjectId turretId=500;
constexpr ObjectId playerId=600;
constexpr ObjectId turretWeaponId=501;
const glm::vec3 muzzle{0.85f,1.65f,-9.05f};

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
        {301,HitKind::Wall,{-6,0,-13},{6,4,-12.8f}},
        {turretWeaponId,HitKind::Weapon,{0.65f,1.45f,-9.5f},{1.05f,1.85f,-9.05f}}
    }};
    if (!environment.empty()) {
        colliders[1].enabled=colliders[2].enabled=colliders[3].enabled=false;
        colliders.insert(colliders.end(),environment.begin(),environment.end());
    }
    inventory.Reset();
    playerFlights.clear(); enemyFlights.clear();
    result=EncounterResult::Running; phase=TurretPhase::Idle;
    phaseTime=playerCooldown=damageFlash=0;
    health=100; playerShots=enemyShots=hitsTaken=blockedEnemyShots=0;
    aimPoint=glm::vec3(0);
}

void TurretEncounter::SetEnvironment(std::span<const Collider> walls) {
    environment.assign(walls.begin(),walls.end()); Reset();
}

bool TurretEncounter::Fire(const glm::vec3& eye,const glm::vec3& direction)
{
    if (!inventory.Armed() || result!=EncounterResult::Running || playerCooldown>0 || playerFlights.size()>=64) return false;
    auto projectile=MakeProjectile(eye,direction,12.0f,inventory.EquippedId());
    if (!projectile) return false;
    playerFlights.push_back({*projectile,0});
    playerCooldown=0.25f; ++playerShots;
    return true;
}

bool TurretEncounter::DropWeapon(const glm::vec3& eye,const glm::vec3& direction) {
    return result!=EncounterResult::Failed && inventory.Drop(eye,direction);
}
bool TurretEncounter::Interact(const glm::vec3& eye,const glm::vec3& direction) {
    return result!=EncounterResult::Failed && inventory.Interact(eye,direction,colliders);
}
WeaponInteraction TurretEncounter::InteractionHint(const glm::vec3& eye,const glm::vec3& direction) const {
    return result==EncounterResult::Failed ? WeaponInteraction::None : inventory.Hint(eye,direction,colliders);
}

void TurretEncounter::ToggleCover()
{
    if (environment.empty() && result==EncounterResult::Running) colliders[1].enabled=!colliders[1].enabled;
}

void TurretEncounter::Update(float realDt,float gameDt,const glm::vec3& eye,const glm::vec3& forward)
{
    if (result!=EncounterResult::Running || !std::isfinite(realDt) || !std::isfinite(gameDt)
        || realDt<0 || gameDt<0 || !Finite(eye)) return;
    playerCooldown=std::max(0.0f,playerCooldown-realDt);
    damageFlash=std::max(0.0f,damageFlash-gameDt);
    if (gameDt==0) return;

    // Player shots never collide with the shooter's body. One impact per bullet.
    for (auto& flight:playerFlights) {
        if (auto hit=AdvanceProjectile(flight.projectile,gameDt,colliders)) {
            if (hit->objectId==turretWeaponId) {
                colliders[4].enabled=false; phase=TurretPhase::Disarmed;
            } else if (hit->objectId==turretId) {
                inventory.Break(flight.projectile.sourceWeaponId);
                if (colliders[4].enabled) inventory.SpawnDrop({0.85f,0.12f,-9.3f});
                colliders[4].enabled=false;
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
    auto enemyColliders=colliders;
    const auto bodyIndex=enemyColliders.size();
    enemyColliders.resize(bodyIndex+2);
    enemyColliders[0].enabled=false;
    enemyColliders[4].enabled=false; // Source turret weapon never blocks its own shots.
    enemyColliders[bodyIndex]={playerId,HitKind::Body,eye+glm::vec3(-0.3f,-1.65f,-0.3f),
        eye+glm::vec3(0.3f,0.15f,0.3f)};
    for (auto& flight:enemyFlights) {
        enemyColliders[bodyIndex+1]=inventory.HeldCollider(eye,forward);
        if (auto hit=AdvanceProjectile(flight.projectile,gameDt,enemyColliders)) {
            if (hit->kind==HitKind::Body && hit->objectId==playerId) {
                health=std::max(0,health-25); ++hitsTaken; damageFlash=0.3f;
            } else if (hit->kind==HitKind::Weapon) {
                inventory.Break(hit->objectId); ++blockedEnemyShots;
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

    if (!colliders[4].enabled) { phase=TurretPhase::Disarmed; return; }
    enemyColliders[bodyIndex+1].enabled=false; // Own held gun must not hide the player from AI sight.
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
