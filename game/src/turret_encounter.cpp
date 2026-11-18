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
    support.Reset();
    if(supportEnabled) { colliders.push_back(support.Body()); colliders.push_back(support.Weapon()); }
    if (!environment.empty()) {
        colliders[1].enabled=colliders[2].enabled=colliders[3].enabled=false;
        colliders.insert(colliders.end(),environment.begin(),environment.end());
    }
    drone.Reset();
    if (droneMode) { colliders[0]=drone.Body(); colliders[4]=drone.Weapon(); }
    inventory.Reset(); inventory.SetSupplyPosition(supplyPosition);
    playerFlights.clear(); enemyFlights.clear();
    result=EncounterResult::Running; phase=TurretPhase::Idle;
    phaseTime=playerCooldown=damageFlash=doorProgress=0;
    health=100; playerShots=enemyShots=hitsTaken=blockedEnemyShots=0;
    aimPoint=glm::vec3(0);
}

void TurretEncounter::SetSupportTurret(bool enabled) { supportEnabled=enabled; Reset(); }

void TurretEncounter::SetEnvironment(std::span<const Collider> walls) {
    environment.assign(walls.begin(),walls.end()); Reset();
}

void TurretEncounter::SetExitDoors(ObjectId left,ObjectId right,float distance) {
    leftDoor=left; rightDoor=right; doorDistance=distance; Reset();
}
void TurretEncounter::UpdateDoors(float gameDt) {
    if (!leftDoor || !rightDoor || doorDistance<=0) return;
    doorProgress=std::min(1.0f,doorProgress+gameDt);
    for (auto& box:colliders) {
        if (box.id!=leftDoor && box.id!=rightDoor) continue;
        const auto original=std::find_if(environment.begin(),environment.end(),[&](const Collider& c){return c.id==box.id;});
        if (original==environment.end()) continue;
        const float offset=(box.id==leftDoor ? -1:1)*doorDistance*doorProgress;
        box.minimum=original->minimum+glm::vec3(offset,0,0);
        box.maximum=original->maximum+glm::vec3(offset,0,0);
    }
}

bool TurretEncounter::SetDroneSpawn(const glm::vec3& point,float yawRadians,float patrolCenterX) {
    if(!drone.SetSpawn(point,yawRadians,patrolCenterX)) return false;
    Reset(); return true;
}
bool TurretEncounter::SetSupply(const glm::vec3& point) {
    if(!Finite(point)) return false;
    supplyPosition=point; Reset(); return true;
}
void TurretEncounter::SetDroneMode(bool enabled) { droneMode=enabled; Reset(); }

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
    if (environment.empty() && (result==EncounterResult::Running || result==EncounterResult::Securing)) colliders[1].enabled=!colliders[1].enabled;
}

void TurretEncounter::Update(float realDt,float gameDt,const glm::vec3& eye,const glm::vec3& forward)
{
    if (result==EncounterResult::Failed || !std::isfinite(realDt) || !std::isfinite(gameDt)
        || realDt<0 || gameDt<0 || !Finite(eye)) return;
    playerCooldown=std::max(0.0f,playerCooldown-realDt);
    damageFlash=std::max(0.0f,damageFlash-gameDt);
    if (gameDt==0) return;
    if (result==EncounterResult::Cleared) { UpdateDoors(gameDt); return; }

    bool droneShot=false;
    if (droneMode && EnemyAlive()) {
        std::vector<Collider> walls;
        for (const auto& box:colliders) if (box.enabled && (box.kind==HitKind::Wall || box.kind==HitKind::Armor)) walls.push_back(box);
        droneShot=drone.Update(gameDt,eye,walls);
        colliders[0]=drone.Body(); colliders[4]=drone.Weapon();
        phase=drone.Phase()==DronePhase::Warning ? TurretPhase::Warning
            : drone.Phase()==DronePhase::Waiting ? TurretPhase::Cooldown
            : drone.Phase()==DronePhase::Disarmed ? TurretPhase::Disarmed : TurretPhase::Idle;
    }

    // Both enemies use the same world clock and shared scenery.
    bool supportShot=false;
    if(supportEnabled) {
        std::vector<Collider> walls;
        for(const auto& box:colliders) if(box.enabled && (box.kind==HitKind::Wall || box.kind==HitKind::Armor)) walls.push_back(box);
        supportShot=support.Update(gameDt,eye,walls);
        colliders[5]=support.Body(); colliders[6]=support.Weapon();
    }
    // Player shots never collide with the shooter's body. One impact per bullet.
    for (auto& flight:playerFlights) {
        if (auto hit=AdvanceProjectile(flight.projectile,gameDt,colliders)) {
            if(supportEnabled && hit->objectId==FixedTurret::WeaponId) {
                support.Disarm(); colliders[6]=support.Weapon();
            } else if(supportEnabled && hit->objectId==FixedTurret::BodyId) {
                inventory.Break(flight.projectile.sourceWeaponId);
                if(support.Armed()) inventory.SpawnDrop(support.Position()+glm::vec3(0,0.12f,0.9f));
                support.Destroy(); colliders[5]=support.Body();colliders[6]=support.Weapon();
            } else if (hit->objectId==turretWeaponId) {
                colliders[4].enabled=false; phase=TurretPhase::Disarmed;
                if (droneMode) drone.Disarm();
            } else if (hit->objectId==turretId) {
                inventory.Break(flight.projectile.sourceWeaponId);
                if (colliders[4].enabled) {
                    const auto drop=droneMode ? drone.Muzzle() : glm::vec3(0.85f,0.12f,-9.3f);
                    inventory.SpawnDrop({drop.x,0.12f,drop.z});
                }
                if (droneMode) drone.Destroy();
                colliders[4].enabled=false;
                colliders[0].enabled=false;
                phase=TurretPhase::Destroyed;
                if(RemainingEnemies()==0) result=EncounterResult::Securing;
            }
        }
        flight.age+=gameDt;
    }
    RemoveExpired(playerFlights);
    if(RemainingEnemies()==0) result=EncounterResult::Securing;

    // Same environment proxies, but enemy shots ignore their source turret.
    auto enemyColliders=colliders;
    const auto bodyIndex=enemyColliders.size();
    enemyColliders.resize(bodyIndex+2);
    enemyColliders[0].enabled=false;
    enemyColliders[4].enabled=false; // Source turret weapon never blocks its own shots.
    enemyColliders[bodyIndex]={playerId,HitKind::Body,eye+glm::vec3(-0.3f,-1.65f,-0.3f),
        eye+glm::vec3(0.3f,0.15f,0.3f)};
    for (auto& flight:enemyFlights) {
        // A projectile ignores only the enemy that fired it.
        const bool fromSupport=flight.projectile.sourceWeaponId==FixedTurret::ShotId;
        enemyColliders[0].enabled=fromSupport && colliders[0].enabled;
        enemyColliders[4].enabled=fromSupport && colliders[4].enabled;
        if(supportEnabled) {
            enemyColliders[5].enabled=!fromSupport && colliders[5].enabled;
            enemyColliders[6].enabled=!fromSupport && colliders[6].enabled;
        }
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

    if(supportEnabled && supportShot && support.Armed()) {
        if(auto shot=MakeProjectile(support.Muzzle(),support.AimPoint()-support.Muzzle(),7.0f,FixedTurret::ShotId)) {
            enemyFlights.push_back({*shot,0}); ++enemyShots;
        }
    }
    if (RemainingEnemies()==0) {
        phase=TurretPhase::Destroyed;
        if (enemyFlights.empty()) { result=EncounterResult::Cleared; playerFlights.clear(); }
        return;
    }
    if(!EnemyAlive()) return;
    if (!colliders[4].enabled) { phase=TurretPhase::Disarmed; return; }
    enemyColliders[0].enabled=enemyColliders[4].enabled=false;
    if(supportEnabled) { enemyColliders[5]=colliders[5]; enemyColliders[6]=colliders[6]; }
    enemyColliders[bodyIndex+1].enabled=false; // Own held gun must not hide the player from AI sight.
    if (droneMode) {
        if (droneShot && drone.Armed()) {
            if (auto shot=MakeProjectile(drone.Muzzle(),drone.AimPoint()-drone.Muzzle(),7.0f,900)) {
                enemyFlights.push_back({*shot,0}); ++enemyShots;
            }
        }
        return;
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
