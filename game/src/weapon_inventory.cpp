#include "weapon_inventory.h"
#include <algorithm>
#include <cmath>
#include <glm/geometric.hpp>

namespace aegis::combat {
namespace {
constexpr ObjectId supplyId=0xffffffffu;
bool Finite(const glm::vec3& v) {
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}
bool Direction(const glm::vec3& v) {
    const auto length2=glm::dot(v,v);
    return Finite(v) && std::isfinite(length2) && length2>1e-8f;
}
}

void WeaponInventory::Reset() {
    equipped=1; nextId=2; drops.clear(); breaks=supplies=pickups=0;
}

bool WeaponInventory::Break(ObjectId sourceId) {
    if (!Armed() || equipped!=sourceId) return false;
    equipped=0; ++breaks; return true;
}

bool WeaponInventory::Drop(const glm::vec3& eye,const glm::vec3& direction) {
    if (!Armed() || !Finite(eye) || !Direction(direction) || drops.size()>=64) return false;
    auto position=eye+glm::normalize(direction)*0.9f;
    position.y=0.12f; // Fixed-floor test arena; world physics is external.
    drops.push_back({equipped,position}); equipped=0; return true;
}

bool WeaponInventory::SpawnDrop(const glm::vec3& position) {
    if (!Finite(position) || drops.size()>=64 || nextId==supplyId) return false;
    drops.push_back({nextId++,position}); return true;
}

ObjectId WeaponInventory::FindInteraction(const glm::vec3& eye,const glm::vec3& direction,
    std::span<const Collider> obstacles) const {
    if (Armed() || !Finite(eye) || !Direction(direction)) return 0;
    const auto end=eye+glm::normalize(direction)*2.5f;
    std::vector<Collider> candidates;
    candidates.push_back({supplyId,HitKind::Weapon,SupplyPosition()-glm::vec3(0.3f,0.25f,0.25f),
        SupplyPosition()+glm::vec3(0.3f,0.25f,0.25f)});
    for (const auto& drop:drops) candidates.push_back({drop.id,HitKind::Weapon,
        drop.position-glm::vec3(0.25f,0.12f,0.25f),drop.position+glm::vec3(0.25f,0.12f,0.25f)});
    const auto item=FindFirstHit(eye,end,candidates);
    if (!item) return 0;
    // Walls/armor/body occlude interaction; no pickup through scenery or enemies.
    const auto obstruction=FindFirstHit(eye,end,obstacles);
    if (obstruction && obstruction->fraction<=item->fraction) return 0;
    return item->objectId;
}

WeaponInteraction WeaponInventory::Hint(const glm::vec3& eye,const glm::vec3& direction,
    std::span<const Collider> obstacles) const {
    const auto id=FindInteraction(eye,direction,obstacles);
    return id==supplyId ? WeaponInteraction::Supply : id ? WeaponInteraction::Pickup : WeaponInteraction::None;
}

bool WeaponInventory::Interact(const glm::vec3& eye,const glm::vec3& direction,
    std::span<const Collider> obstacles) {
    const auto id=FindInteraction(eye,direction,obstacles);
    if (!id) return false;
    if (id==supplyId) {
        if (nextId==supplyId) return false;
        equipped=nextId++; ++supplies; return true;
    }
    const auto item=std::find_if(drops.begin(),drops.end(),[id](const auto& drop){ return drop.id==id; });
    if (item==drops.end()) return false;
    equipped=item->id; drops.erase(item); ++pickups; return true;
}

Collider WeaponInventory::HeldCollider(const glm::vec3& eye,const glm::vec3& direction) const {
    if (!Armed() || !Finite(eye) || !Direction(direction)) return {0,HitKind::Weapon,{0,0,0},{0,0,0},false};
    const auto forward=glm::normalize(direction);
    auto right=glm::cross(forward,glm::vec3(0,1,0));
    if (glm::dot(right,right)<1e-8f) right={1,0,0}; else right=glm::normalize(right);
    const auto up=glm::normalize(glm::cross(right,forward));
    // Conservative world AABB of the camera-local gun; not an exact mesh collider.
    const auto centre=eye+right*0.16f-up*0.16f+forward*0.5f;
    const auto extent=glm::abs(right)*0.09f+glm::abs(up)*0.10f+glm::abs(forward)*0.20f;
    return {equipped,HitKind::Weapon,centre-extent,centre+extent};
}

} // namespace aegis::combat
