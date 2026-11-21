#include "core_encounter.h"
#include "main/GameMath.h"
#include "combat_map/combat_map_layout.generated.h"
#include <algorithm>
#include <cmath>
#include <glm/geometric.hpp>
#include <glm/trigonometric.hpp>

namespace aegis::combat
{
namespace
{
constexpr ObjectId GuardBodyBase = 62000, GuardShotBase = 62500;
constexpr float EnemyShotSpeed = 7;
constexpr int CoreHitDamage = 10, BulletDamage = 25;

ObjectId BodyId(std::size_t guard) { return GuardBodyBase + static_cast<ObjectId>(guard) * 2; }
ObjectId WeaponId(std::size_t guard) { return BodyId(guard) + 1; }

Collider GuardCollider(Collider collider, ObjectId id)
{
    collider.id = id;
    return collider;
}

} // namespace

CoreEncounter::CoreEncounter()
{
    walls.assign(combat_map_asset::playerWalls.begin(), combat_map_asset::playerWalls.end());
    movementWalls = walls;
    Reset();
}

void CoreEncounter::SetEnvironment(std::span<const Collider> shotWalls,
                                   std::span<const Collider> playerWalls)
{
    walls.assign(shotWalls.begin(), shotWalls.end());
    movementWalls.assign(playerWalls.begin(), playerWalls.end());
    Reset();
}

void CoreEncounter::Reset()
{
    ResetCombat(SupplyPosition());
    const float patrolEndZ[] = {-56.8f, -55.8f};
    for (std::size_t i = 0; i < drones.size(); ++i)
    {
        const auto& placement = combat_map_asset::friendDrones[i + 2];
        drones[i].SetSpawn(placement.position, glm::radians(placement.rotYDegrees), patrolEndZ[i]);
        drones[i].SetScale(placement.scale);
    }
    turrets[0].Configure(TurretType::Twin, {4.4f, 0, -60.9f}, 0, 3.28f);
    turrets[1].Configure(TurretType::Twin, {11.6f, 0, -61.2f}, 1, -3.81f);
    phase = MagneticPhase::Dormant;
    phaseTime = 0;
    pulseHit = false;
    coreHealth = 100;
    coreHits = 0;
}

Collider CoreEncounter::CoreTarget()
{
    return {CoreId, HitKind::Body, {6.45f, 0.5f, -57.4175f},
            {9.15f, 5.8f, -54.7175f}};
}

std::vector<Collider> CoreEncounter::Targets() const
{
    auto targets = walls;
    if (CoreAlive()) targets.push_back(CoreTarget());
    for (std::size_t i = 0; i < drones.size(); ++i)
    {
        targets.push_back(GuardCollider(drones[i].Body(), BodyId(i)));
        targets.push_back(GuardCollider(drones[i].Weapon(), WeaponId(i)));
    }
    for (std::size_t i = 0; i < turrets.size(); ++i)
    {
        targets.push_back(GuardCollider(turrets[i].Body(), BodyId(i + 2)));
        targets.push_back(GuardCollider(turrets[i].Weapon(), WeaponId(i + 2)));
        targets.push_back(GuardCollider(turrets[i].Weapon(1), WeaponId(i + 2)));
    }
    return targets;
}

std::vector<Collider> CoreEncounter::PlayerObstacles() const
{
    auto obstacles = movementWalls;
    // 코어의 바닥 장치까지 포함한 예약 영역에 플레이어가 들어가지 못하게 한다.
    obstacles.push_back({CoreId, HitKind::Armor, {5.75f, 0, -58.1175f},
                          {9.85f, 6.4f, -54.0175f}});
    for (std::size_t i = 0; i < drones.size(); ++i)
    {
        auto body = GuardCollider(drones[i].Body(), BodyId(i));
        body.kind = HitKind::Armor;
        obstacles.push_back(body);
    }
    for (std::size_t i = 0; i < turrets.size(); ++i)
    {
        auto body = GuardCollider(turrets[i].Body(), BodyId(i + 2));
        body.kind = HitKind::Armor;
        obstacles.push_back(body);
    }
    return obstacles;
}

int CoreEncounter::RemainingEnemies() const
{
    int count = CoreAlive() ? 1 : 0;
    for (const auto& drone : drones)
        if (drone.Phase() != DronePhase::Destroyed) ++count;
    for (const auto& turret : turrets)
        if (turret.Alive()) ++count;
    return count;
}

std::span<const Collider> CoreEncounter::InteractionColliders(std::vector<Collider>& storage) const
{
    storage = Targets();
    return storage;
}

bool CoreEncounter::DamageCore(int damage)
{
    if (result != EncounterResult::Running || damage <= 0 || !CoreAlive()) return false;
    coreHealth -= std::min(damage, coreHealth);
    ++coreHits;
    if (!CoreAlive())
    {
        // 코어 파괴와 동시에 수호기·자기장을 멈추고 이미 발사된 적탄만 정리한다.
        phase = MagneticPhase::Defeated;
        phaseTime = 0;
        for (auto& drone : drones) drone.Destroy();
        for (auto& turret : turrets) turret.Destroy();
        result = EncounterResult::Securing;
    }
    else if (coreHealth <= 50 && phase == MagneticPhase::Dormant)
    {
        phase = MagneticPhase::Warning;
        phaseTime = 0;
        pulseHit = false;
    }
    return true;
}

void CoreEncounter::UpdatePlayerBullets(float dt)
{
    for (auto& flight : playerFlights)
    {
        if (const auto hit = AdvanceBullet(flight.bullet, dt, Targets()))
        {
            if (hit->objectId == CoreId && CoreAlive())
            {
                DamageCore(CoreHitDamage);
                inventory.Break(flight.bullet.sourceWeaponId);
            }
            for (std::size_t i = 0; i < drones.size(); ++i)
            {
                if (hit->objectId == WeaponId(i)) drones[i].Disarm();
                if (hit->objectId == BodyId(i))
                {
                    inventory.Break(flight.bullet.sourceWeaponId);
                    if (drones[i].Armed())
                        inventory.SpawnDrop({drones[i].Muzzle().x, 0.12f, drones[i].Muzzle().z});
                    drones[i].Destroy();
                }
            }
            for (std::size_t i = 0; i < turrets.size(); ++i)
            {
                if (hit->objectId == WeaponId(i + 2)) turrets[i].Disarm();
                if (hit->objectId == BodyId(i + 2))
                {
                    inventory.Break(flight.bullet.sourceWeaponId);
                    if (turrets[i].Armed())
                        inventory.SpawnDrop(turrets[i].Position() + glm::vec3(0, 0.12f, 0.9f));
                    turrets[i].Destroy();
                }
            }
        }
        flight.age += dt;
    }
    RemoveExpiredFlights(playerFlights);
}

void CoreEncounter::UpdateEnemyBullets(float dt, const glm::vec3& eye, const glm::vec3& forward)
{
    for (auto& flight : enemyFlights)
    {
        auto targets = Targets();
        // 발사한 수호기의 몸·무기는 자기 탄환의 충돌 대상에서 뺀다.
        const auto guard = flight.bullet.sourceWeaponId - GuardShotBase;
        for (auto& target : targets)
            if (target.id == BodyId(guard) || target.id == WeaponId(guard)) target.enabled = false;
        targets.push_back({PlayerId, HitKind::Body, eye + glm::vec3(-0.3f, -1.65f, -0.3f),
                            eye + glm::vec3(0.3f, 0.15f, 0.3f)});
        targets.push_back(inventory.HeldCollider(eye, forward));
        if (const auto hit = AdvanceBullet(flight.bullet, dt, targets))
        {
            if (hit->objectId == PlayerId && hit->kind == HitKind::Body)
            {
                health = std::max(0, health - BulletDamage);
                damageFlash = 0.3f;
            }
            else if (hit->kind == HitKind::Weapon)
                inventory.Break(hit->objectId);
        }
        flight.age += dt;
        if (health == 0) break;
    }
    RemoveExpiredFlights(enemyFlights);
}

void CoreEncounter::SpawnEnemyBullet(const glm::vec3& muzzle, const glm::vec3& target, ObjectId source)
{
    if (enemyFlights.size() >= 128) return;
    if (const auto shot = MakeBullet(muzzle, target - muzzle, EnemyShotSpeed, source))
    {
        enemyFlights.push_back({*shot, 0});
        ++enemyShots;
    }
}

float CoreEncounter::PhaseDuration() const
{
    switch (phase)
    {
    case MagneticPhase::Warning: return magnetic.warningSeconds;
    case MagneticPhase::Pulse: return magnetic.pulseSeconds;
    case MagneticPhase::Recovery: return magnetic.recoverySeconds;
    default: return 0;
    }
}

float CoreEncounter::PhaseProgress() const
{
    const float duration = PhaseDuration();
    return duration > 0 ? std::clamp(phaseTime / duration, 0.0f, 1.0f) : 0;
}

void CoreEncounter::ApplyPulse(const glm::vec3& eye)
{
    const auto delta = eye - CoreCenter();
    const float horizontalDistanceSquared = delta.x * delta.x + delta.z * delta.z;
    if (pulseHit || horizontalDistanceSquared > magnetic.radius * magnetic.radius ||
        FindFirstHit(CoreCenter(), eye - glm::vec3(0, 0.6f, 0), walls)) return;
    health = std::max(0, health - magnetic.damage);
    damageFlash = 0.3f;
    pulseHit = true;
}

void CoreEncounter::UpdateMagnetic(float dt, const glm::vec3& eye)
{
    if (phase == MagneticPhase::Dormant || phase == MagneticPhase::Defeated) return;
    double remaining = dt;
    while (remaining > 0 && health > 0)
    {
        // 프레임 하나가 여러 주기보다 길어도 일정한 시간 안에 처리한다.
        const double cycle = magnetic.warningSeconds + magnetic.pulseSeconds + magnetic.recoverySeconds;
        if (phase == MagneticPhase::Warning && phaseTime == 0 && remaining >= cycle)
        {
            const int pulses = static_cast<int>(std::min(std::floor(remaining / cycle), 5.0));
            for (int i = 0; i < pulses && health > 0; ++i)
            {
                pulseHit = false;
                ApplyPulse(eye);
            }
            remaining = std::fmod(remaining, cycle);
            pulseHit = false;
            continue;
        }
        if (phase == MagneticPhase::Pulse) ApplyPulse(eye);
        const double step = std::min(remaining, static_cast<double>(PhaseDuration() - phaseTime));
        phaseTime += static_cast<float>(step);
        remaining -= step;
        if (phaseTime >= PhaseDuration())
        {
            phaseTime = 0;
            if (phase == MagneticPhase::Warning)
            {
                phase = MagneticPhase::Pulse;
                pulseHit = false;
                ApplyPulse(eye);
            }
            else if (phase == MagneticPhase::Pulse) phase = MagneticPhase::Recovery;
            else phase = MagneticPhase::Warning;
        }
    }
}

void CoreEncounter::Update(float realDeltaTime, float gameDeltaTime, const glm::vec3& eye,
                           const glm::vec3& forward)
{
    if (result == EncounterResult::Failed || result == EncounterResult::Cleared ||
        !std::isfinite(realDeltaTime) || !std::isfinite(gameDeltaTime) ||
        realDeltaTime < 0 || gameDeltaTime < 0 || !IsFinite(eye) || !IsFinite(forward)) return;
    UpdateShotCooldown(realDeltaTime);
    damageFlash = std::max(0.0f, damageFlash - gameDeltaTime);
    if (gameDeltaTime == 0) return;

    std::array<bool, 2> droneShots{}, turretShots{};
    if (result == EncounterResult::Running)
    {
        auto blockers = walls;
        auto core = CoreTarget();
        core.kind = HitKind::Armor;
        blockers.push_back(core);
        for (std::size_t i = 0; i < drones.size(); ++i)
        {
            droneShots[i] = drones[i].Update(gameDeltaTime, eye, blockers);
            turretShots[i] = turrets[i].Update(gameDeltaTime, eye, blockers);
        }
    }
    // 플레이어 명중을 먼저 적용해 방금 파괴된 수호기가 발사하지 않게 한다.
    UpdatePlayerBullets(gameDeltaTime);
    if (result == EncounterResult::Running) UpdateMagnetic(gameDeltaTime, eye);
    UpdateEnemyBullets(gameDeltaTime, eye, forward);
    if (health == 0)
    {
        result = EncounterResult::Failed;
        playerFlights.clear();
        enemyFlights.clear();
        return;
    }
    if (result == EncounterResult::Running)
    {
        for (std::size_t i = 0; i < drones.size(); ++i)
        {
            if (droneShots[i] && drones[i].Armed())
                SpawnEnemyBullet(drones[i].Muzzle(), drones[i].AimPoint(),
                                 GuardShotBase + static_cast<ObjectId>(i));
            if (turretShots[i] && turrets[i].Armed())
                SpawnEnemyBullet(turrets[i].Muzzle(), turrets[i].AimPoint(),
                                 GuardShotBase + static_cast<ObjectId>(i + 2));
        }
    }
    if (result == EncounterResult::Securing && enemyFlights.empty())
    {
        result = EncounterResult::Cleared;
        playerFlights.clear();
    }
}
} // namespace aegis::combat
