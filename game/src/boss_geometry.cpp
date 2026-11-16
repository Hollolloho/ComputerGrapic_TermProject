#include "boss_geometry.h"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>

namespace aegis::combat {
namespace {
struct Triangle { glm::vec3 a,b,c; };
const Triangle triangles[]={
#include "boss_triangles.generated.inc"
};
std::optional<float> Intersect(const glm::vec3& from,const glm::vec3& to,const Triangle& t) {
    const auto d=to-from,e1=t.b-t.a,e2=t.c-t.a;
    const auto p=glm::cross(d,e2); const float det=glm::dot(e1,p);
    if (std::abs(det)<1e-8f) return {};
    const auto s=from-t.a; const float u=glm::dot(s,p)/det;
    if (u<0 || u>1) return {};
    const auto q=glm::cross(s,e1); const float v=glm::dot(d,q)/det;
    if (v<0 || u+v>1) return {};
    const float f=glm::dot(e2,q)/det;
    return f>=0 && f<=1 ? std::optional<float>(f):std::nullopt;
}
}
BossPose MakeBossPose(float armorOpen,const glm::vec3& aim) {
    BossPose pose;
    for(std::size_t i=0;i<pose.nodes.size();++i) {
        const auto& node=boss_asset::nodes[i];
        const auto parent=node.parent<0 ? glm::translate(glm::mat4(1),boss_asset::mount):pose.nodes[node.parent];
        auto offset=node.translation;
        if (i==boss_asset::lower_armor_L_slide) offset.x-=boss_asset::armorTravel*armorOpen;
        if (i==boss_asset::lower_armor_R_slide) offset.x+=boss_asset::armorTravel*armorOpen;
        auto model=glm::translate(parent,offset);
        auto angles=node.rotation;
        if (i==boss_asset::turret_L_yaw || i==boss_asset::turret_R_yaw) {
            const auto d=aim-glm::vec3(model[3]);
            angles.y=std::clamp(glm::degrees(std::atan2(d.x,d.z)),-25.0f,25.0f);
        }
        if (i==boss_asset::turret_L_pitch || i==boss_asset::turret_R_pitch) {
            const auto local=glm::vec3(glm::inverse(parent)*glm::vec4(aim,1));
            angles.x=std::clamp(glm::degrees(std::atan2(-local.y,std::sqrt(local.x*local.x+local.z*local.z))),-10.0f,35.0f);
        }
        model=glm::rotate(model,glm::radians(angles.x),{1,0,0});
        model=glm::rotate(model,glm::radians(angles.y),{0,1,0});
        model=glm::rotate(model,glm::radians(angles.z),{0,0,1});
        pose.nodes[i]=model;
    }
    for(std::size_t i=0;i<pose.parts.size();++i) pose.parts[i]=pose.nodes[boss_asset::parts[i].node];
    return pose;
}
glm::vec3 BossCoreCenter(const BossPose& pose) { return glm::vec3(pose.nodes[boss_asset::core_visual]*glm::vec4(boss_asset::weakCenter,1)); }
glm::vec3 BossMuzzle(const BossPose& pose,int side) { return glm::vec3(pose.nodes[side ? boss_asset::muzzle_R:boss_asset::muzzle_L][3]); }
std::optional<Hit> FindBossHit(const glm::vec3& start,const glm::vec3& end,const BossPose& pose,bool exposed) {
    const auto centre=BossCoreCenter(pose);
    const Collider weak{21000,exposed ? HitKind::Body:HitKind::Armor,centre-boss_asset::weakHalf,centre+boss_asset::weakHalf};
    auto best=FindFirstHit(start,end,std::span<const Collider>(&weak,1));
    for(std::size_t i=0;i<pose.parts.size();++i) {
        if (i==boss_asset::part_core_crystal) continue;
        const auto& part=boss_asset::parts[i];
        const auto inverse=glm::inverse(pose.parts[i]);
        const auto a=glm::vec3(inverse*glm::vec4(start,1)),b=glm::vec3(inverse*glm::vec4(end,1));
        const Collider broad{static_cast<ObjectId>(20000+i),HitKind::Armor,part.low,part.high};
        if (!FindFirstHit(a,b,std::span<const Collider>(&broad,1))) continue;
        for(int index=part.start;index<part.start+part.triangles;++index) {
            if (const auto f=Intersect(a,b,triangles[index]); f && (!best || *f<=best->fraction))
                best=Hit{broad.id,HitKind::Armor,*f,start+(end-start)*(*f)};
        }
    }
    return best;
}
}
