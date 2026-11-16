#pragma once

#include "combat.h"
#include <vector>

namespace aegis::combat {

struct DroppedWeapon { ObjectId id; glm::vec3 position; };
enum class WeaponInteraction { None, Pickup, Supply };

// One equipped instance. IDs travel with projectiles and intact dropped weapons.
class WeaponInventory {
public:
    void Reset();
    bool Armed() const { return equipped!=0; }
    ObjectId EquippedId() const { return equipped; }
    bool Break(ObjectId sourceId);
    bool Drop(const glm::vec3& eye,const glm::vec3& direction);
    bool SpawnDrop(const glm::vec3& position);
    WeaponInteraction Hint(const glm::vec3& eye,const glm::vec3& direction,
        std::span<const Collider> obstacles) const;
    bool Interact(const glm::vec3& eye,const glm::vec3& direction,
        std::span<const Collider> obstacles);
    const std::vector<DroppedWeapon>& Drops() const { return drops; }
    int Breaks() const { return breaks; }
    int Supplies() const { return supplies; }
    int Pickups() const { return pickups; }
    static glm::vec3 SupplyPosition() { return {-0.8f,1.15f,-1.4f}; }
    Collider HeldCollider(const glm::vec3& eye,const glm::vec3& direction) const;

private:
    ObjectId FindInteraction(const glm::vec3& eye,const glm::vec3& direction,
        std::span<const Collider> obstacles) const;
    ObjectId equipped{1},nextId{2};
    std::vector<DroppedWeapon> drops;
    int breaks{},supplies{},pickups{};
};

} // namespace aegis::combat
