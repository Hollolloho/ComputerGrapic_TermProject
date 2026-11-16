#include "boss_encounter.h"
#include <algorithm>
#include <cmath>
#include <glm/geometric.hpp>

namespace aegis::combat {
namespace {
bool Finite(const glm::vec3& v) { return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z); }
void Expire(std::vector<Flight>& flights) {
    std::erase_if(flights,[](const Flight& f){return !f.projectile.active || f.age>=4;});
}
}
BossEncounter::BossEncounter() { Reset(); }
void BossEncounter::Reset() {
    inventory.Reset(); inventory.SetSupplyPosition(boss_asset::supplyInteraction);
    playerFlights.clear(); enemyFlights.clear();
    result=EncounterResult::Running; phase=BossPhase::Warning;
    aimPoint=FiringRoot()+glm::vec3(0,1.05f,0);
    timer=1.2f; armorOpen=cooldown=damageFlash=0;
    health=100; coreHits=playerShots=enemyShots=blockedShots=0;
    pose=MakeBossPose(0,aimPoint);
}
bool BossEncounter::Fire(const glm::vec3& eye,const glm::vec3& direction) {
    if (result!=EncounterResult::Running || !inventory.Armed() || cooldown>0 || playerFlights.size()>=64) return false;
    const auto shot=MakeProjectile(eye,direction,12,inventory.EquippedId());
    if (!shot) return false;
    playerFlights.push_back({*shot,0}); cooldown=0.25f; ++playerShots; return true;
}
bool BossEncounter::DropWeapon(const glm::vec3& eye,const glm::vec3& direction) {
    return result!=EncounterResult::Failed && inventory.Drop(eye,direction);
}
bool BossEncounter::Interact(const glm::vec3& eye,const glm::vec3& direction) {
    return result!=EncounterResult::Failed && inventory.Interact(eye,direction,boss_asset::walls);
}
WeaponInteraction BossEncounter::InteractionHint(const glm::vec3& eye,const glm::vec3& direction) const {
    return result==EncounterResult::Failed ? WeaponInteraction::None : inventory.Hint(eye,direction,boss_asset::walls);
}
void BossEncounter::Shoot(const glm::vec3& eye) {
    for(int side=0;side<2;++side) {
        const auto muzzle=BossMuzzle(pose,side);
        // The physical cabinets/room block firing toward a covered player.
        if (FindFirstHit(muzzle,eye-glm::vec3(0,0.6f,0),boss_asset::walls)) continue;
        const auto node=side ? boss_asset::muzzle_R:boss_asset::muzzle_L;
        if (auto shot=MakeProjectile(muzzle,glm::vec3(pose.nodes[node][2]),7,23000+side)) {
            enemyFlights.push_back({*shot,0}); ++enemyShots;
        }
    }
}
void BossEncounter::AdvancePattern(float dt,const glm::vec3& eye) {
    if (phase==BossPhase::Defeated) return;
    if (phase==BossPhase::Warning) aimPoint=eye-glm::vec3(0,0.6f,0);
    timer-=dt;
    if (phase==BossPhase::Opening) armorOpen=std::clamp(1-timer/0.75f,0.0f,1.0f);
    if (phase==BossPhase::Closing) armorOpen=std::clamp(timer/0.6f,0.0f,1.0f);
    pose=MakeBossPose(armorOpen,aimPoint);
    if (timer>0) return;
    switch(phase) {
    case BossPhase::Warning: phase=BossPhase::Attack; timer=0.35f; Shoot(eye); break;
    case BossPhase::Attack: phase=BossPhase::Opening; timer=0.75f; break;
    case BossPhase::Opening: phase=BossPhase::Exposed; timer=3; armorOpen=1; break;
    case BossPhase::Exposed: phase=BossPhase::Closing; timer=0.6f; break;
    case BossPhase::Closing: phase=BossPhase::Recovery; timer=1.2f; armorOpen=0; break;
    case BossPhase::Recovery: phase=BossPhase::Warning; timer=1.2f; break;
    case BossPhase::Defeated: break;
    }
    pose=MakeBossPose(armorOpen,aimPoint);
}
void BossEncounter::Update(float realDt,float gameDt,const glm::vec3& eye,const glm::vec3& forward) {
    if (result==EncounterResult::Failed || !Finite(eye)
        || !std::isfinite(realDt)||!std::isfinite(gameDt)||realDt<0||gameDt<0) return;
    cooldown=std::max(0.0f,cooldown-realDt); damageFlash=std::max(0.0f,damageFlash-gameDt);
    if (gameDt==0 || result==EncounterResult::Cleared) return;
    if (result==EncounterResult::Running) AdvancePattern(gameDt,eye);
    for(auto& flight:playerFlights) {
        auto& bullet=flight.projectile;
        const auto end=bullet.position+bullet.velocity*gameDt;
        auto hit=FindFirstHit(bullet.position,end,boss_asset::walls);
        const auto bossHit=FindBossHit(bullet.position,end,pose,phase==BossPhase::Exposed);
        if (bossHit && (!hit || bossHit->fraction<hit->fraction)) hit=bossHit;
        if (bullet.active && hit) {
            bullet.position=hit->position; bullet.active=false;
            if (hit->kind==HitKind::Body && phase==BossPhase::Exposed) {
                ++coreHits; inventory.Break(bullet.sourceWeaponId);
                phase=coreHits>=3 ? BossPhase::Defeated:BossPhase::Closing;
                timer=0.6f; // Core is disabled immediately, even while panels travel shut.
                if (coreHits>=3) result=EncounterResult::Securing;
            } else ++blockedShots;
        } else if (bullet.active) bullet.position=end;
        flight.age+=gameDt;
    }
    Expire(playerFlights);
    for(auto& flight:enemyFlights) {
        std::vector<Collider> boxes(boss_asset::walls.begin(),boss_asset::walls.end());
        boxes.push_back({24000,HitKind::Body,eye+glm::vec3(-0.3f,-1.65f,-0.3f),eye+glm::vec3(0.3f,0.15f,0.3f)});
        boxes.push_back(inventory.HeldCollider(eye,forward));
        if (auto hit=AdvanceProjectile(flight.projectile,gameDt,boxes)) {
            if (hit->kind==HitKind::Body && hit->objectId==24000) {
                health=std::max(0,health-25); damageFlash=0.3f;
            } else if (hit->kind==HitKind::Weapon) inventory.Break(hit->objectId);
        }
        flight.age+=gameDt;
    }
    Expire(enemyFlights);
    if (health==0) { result=EncounterResult::Failed; playerFlights.clear(); enemyFlights.clear(); return; }
    if (result==EncounterResult::Securing && enemyFlights.empty()) { result=EncounterResult::Cleared; playerFlights.clear(); }
}
}
