#include "fixed_turret.h"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>
#include <limits>
namespace aegis::combat {
namespace {
bool Finite(const glm::vec3& v) { return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z); }
}
bool FixedTurret::SetPosition(const glm::vec3& point) {
    if(!Finite(point)) return false;
    position=point; Reset(); return true;
}
void FixedTurret::Reset() { phase=TurretPhase::Idle; timer=0; direction={0,0,1}; aimPoint=position+glm::vec3(0,1.65f,5); }
void FixedTurret::Disarm() { if(Alive()) { phase=TurretPhase::Disarmed; timer=0; } }
void FixedTurret::Destroy() { phase=TurretPhase::Destroyed; timer=0; }
glm::vec3 FixedTurret::Muzzle() const { return position+glm::vec3(0,1.65f,0)+direction*0.85f; }
glm::mat4 FixedTurret::BarrelTransform() const {
    const float yaw=std::atan2(direction.x,direction.z);
    const float pitch=-std::asin(std::clamp(direction.y,-1.0f,1.0f));
    return glm::rotate(glm::rotate(glm::translate(glm::mat4(1),position+glm::vec3(0,1.65f,0)),yaw,glm::vec3(0,1,0)),pitch,glm::vec3(1,0,0));
}
Collider FixedTurret::Body() const { return {BodyId,HitKind::Body,position+glm::vec3(-0.5f,0,-0.5f),position+glm::vec3(0.5f,2.2f,0.5f),Alive()}; }
Collider FixedTurret::Weapon() const {
    glm::vec3 low(std::numeric_limits<float>::max()),high(std::numeric_limits<float>::lowest());
    const auto transform=BarrelTransform();
    for(int i=0;i<8;++i) {
        const auto p=glm::vec3(transform*glm::vec4(i&1 ? 0.14f:-0.14f,i&2 ? 0.14f:-0.14f,i&4 ? 0.8f:-0.1f,1));
        low=glm::min(low,p);high=glm::max(high,p);
    }
    return {WeaponId,HitKind::Weapon,low,high,Armed()};
}
bool FixedTurret::Update(float gameDt,const glm::vec3& eye,std::span<const Collider> walls) {
    if(!Armed() || !std::isfinite(gameDt) || gameDt<=0 || !Finite(eye)) return false;
    if(phase==TurretPhase::Cooldown) { timer-=gameDt; if(timer<=0) phase=TurretPhase::Idle; return false; }
    const bool visible=glm::length(eye-position)<18 && !FindFirstHit(Muzzle(),eye,walls);
    if(!visible) { phase=TurretPhase::Idle; timer=0; return false; }
    if(phase==TurretPhase::Idle) {
        const auto delta=eye-(position+glm::vec3(0,1.65f,0));
        if(glm::dot(delta,delta)<1e-8f) return false;
        direction=glm::normalize(delta); aimPoint=eye; phase=TurretPhase::Warning; timer=1.2f; return false;
    }
    timer-=gameDt;
    if(timer>0) return false;
    phase=TurretPhase::Cooldown; timer=1.8f; return true;
}
}
