#include "combat_range.h"

#include <algorithm>
#include <cmath>

namespace aegis::combat {

CombatRange::CombatRange() { Reset(); }

void CombatRange::Reset()
{
    colliders = {{
        {100, HitKind::Body, {-2.6f,1.1f,-10.2f}, {-1.6f,2.1f,-9.5f}},
        {101, HitKind::Body, {-0.5f,1.1f,-10.2f}, {0.5f,2.1f,-9.5f}},
        {102, HitKind::Body, {1.6f,1.1f,-10.2f}, {2.6f,2.1f,-9.5f}},
        {200, HitKind::Wall, {-0.85f,0,-6.3f}, {0.85f,2.5f,-5.8f}},
        {300, HitKind::Wall, {-6,-0.2f,-14}, {6,0,3}},
        {301, HitKind::Wall, {-6,0,-13}, {6,4,-12.8f}}
    }};
    flights.clear();
    lastHit.reset();
    cooldown = impactTime = 0;
    shots = blockedShots = 0;
}

bool CombatRange::Fire(const glm::vec3& eye, const glm::vec3& direction)
{
    if (cooldown > 0 || flights.size() >= 64) return false;
    auto projectile = MakeProjectile(eye, direction, 12.0f, 7);
    if (!projectile) return false;
    flights.push_back({*projectile, 0});
    cooldown = 0.25f;
    ++shots;
    return true;
}

void CombatRange::Update(float realDt, float gameDt)
{
    if (!std::isfinite(realDt) || !std::isfinite(gameDt) || realDt < 0 || gameDt < 0) return;
    cooldown = std::max(0.0f, cooldown - realDt);
    impactTime = std::max(0.0f, impactTime - gameDt);
    for (auto& flight : flights) {
        if (auto hit = AdvanceProjectile(flight.projectile, gameDt, colliders)) {
            lastHit = hit;
            impactTime = 0.4f;
            if (hit->kind == HitKind::Body) {
                for (auto& collider : colliders) {
                    if (collider.id == hit->objectId) collider.enabled = false;
                }
            } else {
                ++blockedShots;
            }
        }
        flight.age += gameDt;
    }
    std::erase_if(flights, [](const Flight& flight) {
        return !flight.projectile.active || flight.age >= 4.0f;
    });
}

void CombatRange::ToggleCover() { colliders[3].enabled = !colliders[3].enabled; }

int CombatRange::RemainingTargets() const
{
    return static_cast<int>(std::count_if(colliders.begin(), colliders.end(),
        [](const Collider& box) { return box.kind == HitKind::Body && box.enabled; }));
}

} // namespace aegis::combat
