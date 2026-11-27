#include "core_encounter.h"
#include "main/GameMath.h"
#include "combat_map/combat_map_layout.generated.h"
#include "combat_map/interlock_layout.h"
#include <algorithm>
#include <cmath>
#include <glm/geometric.hpp>
#include <glm/trigonometric.hpp>

namespace aegis::combat
{
namespace
{
const ObjectId GuardBodyBase = 62000, GuardShotBase = 62500;
const float EnemyShotSpeed = 7;
const int CoreHitDamage = 10, BulletDamage = 25;

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
    for (auto& wall : walls)
        interlock_asset::MoveCover(wall);
    movementWalls = walls;
    Reset();
}

void CoreEncounter::SetEnvironment(const std::vector<Collider>& shotWalls,
                                   const std::vector<Collider>& playerWalls)
{
    walls = shotWalls;
    movementWalls = playerWalls;
    Reset();
}

void CoreEncounter::Reset()
{
    ResetCombat(SupplyPosition());
    const glm::vec3 dronePositions[DroneCount] = {
        {1.050000f, 3.200000f, -54.009942f}, {0.425000f, 4.000000f, -59.572442f}, {2.862500f, 4.700000f, -63.509942f},
        {14.550000f, 3.200000f, -54.009942f}, {15.175000f, 4.000000f, -58.884942f}, {12.550000f, 4.700000f, -63.759942f}};
    for (std::size_t i = 0; i < drones.size(); ++i)
    {
        const auto& point = dronePositions[i];
        const auto direction = PlayerRoot() - point;
        const float rotY = std::atan2(direction.x, direction.z);
        // 코어실 드론은 제자리에서 입장 직후부터 플레이어를 조준한다.
        drones[i].SetSpawn(point, rotY, point.z);
        drones[i].SetScale(.65f);
        drones[i].StartTracking();
    }
    const glm::vec3 turretPositions[TurretCount] = {
        {3.550000f, 0.000000f, -61.884942f}, {12.675000f, 0.000000f, -59.384942f},
        {2.300000f, 0.000000f, -57.697442f}, {13.925000f, 0.000000f, -55.822442f}};
    for (std::size_t i = 0; i < turrets.size(); ++i)
    {
        const auto& point = turretPositions[i];
        const auto direction = PlayerRoot() - point;
        const float rotY = glm::degrees(std::atan2(direction.x, direction.z));
        turrets[i].Configure(i < 2 ? TurretType::Twin : TurretType::SingleC,
                             point, static_cast<ObjectId>(i), rotY);
    }
    phase = MagneticPhase::Dormant;
    phaseTime = 0;
    pulseChecked = false;
    coreHealth = 100;
    coreHits = 0;
}

Collider CoreEncounter::CoreTarget()
{
    const auto position = combat_map_asset::corePosition;
    return {CoreId, HitKind::Body, position + glm::vec3(-1.35f, .5f, -1.35f),
            position + glm::vec3(1.35f, 5.8f, 1.35f)};
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
        for (auto target : turrets[i].Targets())
        {
            target.id = target.kind == HitKind::Weapon ? WeaponId(i + drones.size()) : BodyId(i + drones.size());
            targets.push_back(target);
        }
    }
    return targets;
}

std::vector<Collider> CoreEncounter::PlayerObstacles() const
{
    auto obstacles = movementWalls;
    // 코어의 바닥 장치까지 포함한 예약 영역에 플레이어가 들어가지 못하게 한다.
    const auto position = combat_map_asset::corePosition;
    obstacles.push_back({CoreId, HitKind::Armor, position + glm::vec3(-2.05f, 0, -2.05f),
                          position + glm::vec3(2.05f, 6.4f, 2.05f)});
    for (std::size_t i = 0; i < drones.size(); ++i)
    {
        auto body = GuardCollider(drones[i].Body(), BodyId(i));
        body.kind = HitKind::Armor;
        obstacles.push_back(body);
    }
    for (std::size_t i = 0; i < turrets.size(); ++i)
    {
        auto body = GuardCollider(turrets[i].Body(), BodyId(i + drones.size()));
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

const std::vector<Collider>& CoreEncounter::InteractionColliders(std::vector<Collider>& storage) const
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
        pulseChecked = false;
    }
    return true;
}

void CoreEncounter::UpdatePlayerBullets(float dt)
{
    for (auto& flight : playerFlights)
    {
        Hit hit;
        if (AdvanceFlight(flight, dt, Targets(), hit))
        {
            ShatterBullet(flight.bullet, false);
            if (hit.objectId == CoreId && CoreAlive())
            {
                DamageCore(CoreHitDamage);
                inventory.Break(flight.bullet.sourceWeaponId);
            }
            for (std::size_t i = 0; i < drones.size(); ++i)
            {
                if (hit.objectId == WeaponId(i)) drones[i].Disarm();
                if (hit.objectId == BodyId(i))
                {
                    inventory.Break(flight.bullet.sourceWeaponId);
                    if (drones[i].Armed())
                        inventory.SpawnDrop({drones[i].Muzzle().x, 0.12f, drones[i].Muzzle().z});
                    drones[i].Destroy(flight.bullet.velocity);
                }
            }
            for (std::size_t i = 0; i < turrets.size(); ++i)
            {
                const bool bodyHit = hit.objectId == BodyId(i + drones.size());
                if (bodyHit || hit.objectId == WeaponId(i + drones.size()))
                {
                    inventory.Break(flight.bullet.sourceWeaponId);
                    if (bodyHit && turrets[i].Armed())
                        inventory.SpawnDrop(turrets[i].Position() + glm::vec3(0, 0.12f, 0.9f));
                    turrets[i].Destroy();
                }
            }
        }
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
        targets.push_back(PlayerBodyCollider(PlayerId, eye));
        targets.push_back(inventory.HeldCollider(eye, forward));
        Hit hit;
        if (AdvanceFlight(flight, dt, targets, hit))
        {
            ShatterBullet(flight.bullet, true);
            if (hit.objectId == PlayerId && hit.kind == HitKind::Body)
            {
                health = std::max(0, health - BulletDamage);
                damageFlash = 0.3f;
            }
            else if (hit.kind == HitKind::Weapon)
                inventory.Break(hit.objectId);
        }
        if (health == 0) break;
    }
    RemoveExpiredFlights(enemyFlights);
}

void CoreEncounter::SpawnEnemyBullet(const glm::vec3& muzzle, const glm::vec3& target, ObjectId source, BulletStyle bulletStyle)
{
    if (enemyFlights.size() >= 128) return;
    Bullet shot;
    if (MakeBullet(muzzle, target - muzzle, EnemyShotSpeed, source, shot))
    {
        enemyFlights.push_back({shot, 0, bulletStyle});
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

void CoreEncounter::ApplyPulse(const glm::vec3& eye, float waveRadius)
{
    const auto delta = eye - CoreCenter();
    const float horizontalDistanceSquared = delta.x * delta.x + delta.z * delta.z;
    if (pulseChecked || horizontalDistanceSquared > waveRadius * waveRadius) return;
    pulseChecked = true;

    // 코어 하단에서 가슴 높이로 검사해 낮은 엄폐물도 자기장을 막게 한다.
    glm::vec3 origin = CoreCenter();
    origin.y = .9f;
    if (IsBlocked(origin, eye - glm::vec3(0, .6f, 0), walls)) return;
    health = std::max(0, health - magnetic.damage);
    damageFlash = 0.3f;
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
                pulseChecked = false;
                ApplyPulse(eye, magnetic.radius);
            }
            remaining = std::fmod(remaining, cycle);
            pulseChecked = false;
            continue;
        }
        const double step = std::min(remaining, static_cast<double>(PhaseDuration() - phaseTime));
        phaseTime += static_cast<float>(step);
        remaining -= step;
        // 화면의 파동과 같은 반경까지 도달했을 때 한 번만 엄폐·피해를 판정한다.
        if (phase == MagneticPhase::Pulse)
            ApplyPulse(eye, magnetic.radius * PhaseProgress());
        if (phaseTime >= PhaseDuration())
        {
            phaseTime = 0;
            if (phase == MagneticPhase::Warning)
            {
                phase = MagneticPhase::Pulse;
                pulseChecked = false;
            }
            else if (phase == MagneticPhase::Pulse) phase = MagneticPhase::Recovery;
            else phase = MagneticPhase::Warning;
        }
    }
}

void CoreEncounter::Update(float realDeltaTime, float gameDeltaTime, const glm::vec3& eye,
                           const glm::vec3& forward)
{
    if (!std::isfinite(realDeltaTime) || !std::isfinite(gameDeltaTime) ||
        realDeltaTime < 0 || gameDeltaTime < 0 || !IsFinite(eye) || !IsFinite(forward)) return;
    UpdateBulletEffects(gameDeltaTime);
    if (result == EncounterResult::Failed || result == EncounterResult::Cleared) return;
    UpdateShotCooldown(realDeltaTime);
    damageFlash = std::max(0.0f, damageFlash - gameDeltaTime);
    if (gameDeltaTime == 0) return;

    std::array<bool, DroneCount> droneShots{};
    std::array<bool, TurretCount> turretShots{};
    if (result == EncounterResult::Running)
    {
        auto blockers = walls;
        auto core = CoreTarget();
        core.kind = HitKind::Armor;
        blockers.push_back(core);
        for (std::size_t i = 0; i < drones.size(); ++i)
            droneShots[i] = drones[i].Update(gameDeltaTime, eye, blockers);
        for (std::size_t i = 0; i < turrets.size(); ++i)
            turretShots[i] = turrets[i].Update(gameDeltaTime, eye, blockers);
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
                                 GuardShotBase + static_cast<ObjectId>(i), BulletStyle::Drone);
        }
        for (std::size_t i = 0; i < turrets.size(); ++i)
        {
            if (turretShots[i] && turrets[i].Armed())
                SpawnEnemyBullet(turrets[i].Muzzle(), turrets[i].AimPoint(),
                                 GuardShotBase + static_cast<ObjectId>(i + drones.size()), BulletStyle::Turret);
        }
    }
    if (result == EncounterResult::Securing && enemyFlights.empty())
    {
        result = EncounterResult::Cleared;
        playerFlights.clear();
    }
}
} // namespace aegis::combat
