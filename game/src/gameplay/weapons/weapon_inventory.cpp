// 장착 총, 떨어진 총, 보급과 총 파손을 관리한다. 무기 ID로 같은 총을 중복 처리하지 않는다.
#include "weapon_inventory.h"
#include "main/GameMath.h"
#include <cmath>
#include <glm/geometric.hpp>

namespace aegis::combat
{
namespace
{
const ObjectId supplyId = 0xffffffffu;

bool Direction(const glm::vec3& v)
{
    const auto length2 = glm::dot(v, v);
    return IsFinite(v) && std::isfinite(length2) && length2 > 1e-8f;
}
} // namespace

void WeaponInventory::Reset()
{
    equipped = 1;
    nextId = 2;
    drops.clear();
    breaks = supplies = pickups = 0;
}

bool WeaponInventory::SetSupplyPosition(const glm::vec3& point)
{
    if (!IsFinite(point))
    {
        return false;
    }
    supplyPosition = point;
    return true;
}

bool WeaponInventory::Break(ObjectId sourceId)
{
    if (!Armed() || equipped != sourceId)
    {
        return false;
    }
    equipped = 0;
    ++breaks;
    return true;
}

bool WeaponInventory::Drop(const glm::vec3& eye, const glm::vec3& direction)
{
    if (!Armed() || !IsFinite(eye) || !Direction(direction) || drops.size() >= 64)
    {
        return false;
    }
    auto position = eye + glm::normalize(direction) * 0.9f;
    position.y = 0.12f; // Fixed-floor test arena; world physics is external.
    drops.push_back({equipped, position});
    equipped = 0;
    return true;
}

bool WeaponInventory::SpawnDrop(const glm::vec3& position)
{
    if (!IsFinite(position) || drops.size() >= 64 || nextId == supplyId)
    {
        return false;
    }
    drops.push_back({nextId++, position});
    return true;
}

bool WeaponInventory::FindInteraction(const glm::vec3& eye, const glm::vec3& direction,
                                      const std::vector<Collider>& obstacles, Hit& item) const
{
    if (Armed() || !IsFinite(eye) || !Direction(direction))
    {
        return false;
    }
    const auto end = eye + glm::normalize(direction) * 2.5f;
    std::vector<Collider> candidates;
    candidates.push_back({supplyId, HitKind::Weapon, supplyPosition - glm::vec3(0.3f, 0.25f, 0.25f),
                          supplyPosition + glm::vec3(0.3f, 0.25f, 0.25f)});
    for (const auto& drop : drops)
    {
        candidates.push_back({drop.id, HitKind::Weapon, drop.position - glm::vec3(0.25f, 0.12f, 0.25f),
                              drop.position + glm::vec3(0.25f, 0.12f, 0.25f)});
    }
    if (!FindFirstHit(eye, end, candidates, item))
    {
        return false;
    }
    // Walls/armor/body occlude interaction; no pickup through scenery or enemies.
    Hit obstruction;
    if (FindFirstHit(eye, end, obstacles, obstruction) && obstruction.fraction <= item.fraction)
    {
        return false;
    }
    return true;
}

float WeaponInventory::InteractionDistance(const glm::vec3& eye, const glm::vec3& direction,
                                           const std::vector<Collider>& obstacles) const
{
    Hit hit;
    if (!FindInteraction(eye, direction, obstacles, hit))
    {
        return -1.0f;
    }
    return hit.fraction * 2.5f;
}

WeaponInteraction WeaponInventory::Hint(const glm::vec3& eye, const glm::vec3& direction,
                                        const std::vector<Collider>& obstacles) const
{
    Hit hit;
    if (!FindInteraction(eye, direction, obstacles, hit) || hit.objectId == 0)
    {
        return WeaponInteraction::None;
    }
    if (hit.objectId == supplyId)
    {
        return WeaponInteraction::Supply;
    }
    return WeaponInteraction::Pickup;
}

bool WeaponInventory::Interact(const glm::vec3& eye, const glm::vec3& direction,
                               const std::vector<Collider>& obstacles)
{
    Hit hit;
    if (!FindInteraction(eye, direction, obstacles, hit) || hit.objectId == 0)
    {
        return false;
    }
    const ObjectId id = hit.objectId;
    if (id == supplyId)
    {
        if (nextId == supplyId)
        {
            return false;
        }
        equipped = nextId++;
        ++supplies;
        return true;
    }
    // 바닥의 총을 순서대로 찾고, 선택한 총만 목록에서 지운다.
    for (std::size_t index = 0; index < drops.size(); ++index)
    {
        if (drops[index].id == id)
        {
            equipped = drops[index].id;
            drops.erase(drops.begin() + index);
            ++pickups;
            return true;
        }
    }
    return false;
}

Collider WeaponInventory::HeldCollider(const glm::vec3& eye, const glm::vec3& direction) const
{
    if (!Armed() || !IsFinite(eye) || !Direction(direction))
    {
        return {0, HitKind::Weapon, {0, 0, 0}, {0, 0, 0}, false};
    }
    const auto forward = glm::normalize(direction);
    auto right = glm::cross(forward, glm::vec3(0, 1, 0));
    if (glm::dot(right, right) < 1e-8f)
    {
        right = {1, 0, 0};
    }
    else
    {
        right = glm::normalize(right);
    }
    const auto up = glm::normalize(glm::cross(right, forward));
    // Conservative world AABB of the camera-local gun; not an exact mesh collider.
    const auto centre = eye + right * 0.16f - up * 0.16f + forward * 0.5f;
    const auto extent = glm::abs(right) * 0.09f + glm::abs(up) * 0.10f + glm::abs(forward) * 0.20f;
    return {equipped, HitKind::Weapon, centre - extent, centre + extent};
}

} // namespace aegis::combat
