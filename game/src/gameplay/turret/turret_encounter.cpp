// 드론·포탑 전투의 상태를 관리한다. Update에서 처리 순서를 확인할 수 있다.
#include "turret_encounter.h"
#include "main/GameMath.h"

#include <algorithm>
#include <cmath>

namespace aegis::combat
{
namespace
{
const ObjectId turretId = 500;
const ObjectId playerId = 600;
const ObjectId turretWeaponId = 501;
const ObjectId ExtraDroneBodyId = 20000;
const ObjectId ExtraDroneShotId = 22000;
const glm::vec3 muzzle{0.85f, 1.65f, -9.05f};

} // namespace

TurretEncounter::TurretEncounter()
{
    Reset();
}

void TurretEncounter::Reset()
{
    // 각 포탑의 받침·머리·코어·포신 충돌 상자를 연속으로 둔다.
    colliders = {{{turretId, HitKind::Body, {-0.6f, 0, -10.4f}, {0.6f, 2.25f, -9.5f}},
                  {200, HitKind::Wall, {-0.85f, 0, -6.3f}, {0.85f, 2.5f, -5.8f}, false},
                  {300, HitKind::Wall, {-6, -0.2f, -14}, {6, 0, 3}},
                  {301, HitKind::Wall, {-6, 0, -13}, {6, 4, -12.8f}},
                  {turretWeaponId, HitKind::Weapon, {0.65f, 1.45f, -9.5f}, {1.05f, 1.85f, -9.05f}}}};
    for (auto& turret : turrets)
    {
        turret.Reset();
        const auto targets = turret.Targets();
        colliders.insert(colliders.end(), targets.begin(), targets.end());
    }
    colliders.resize(colliders.size() + 2 * extraDrones.size());
    for (std::size_t i = 0; i < extraDrones.size(); ++i)
    {
        extraDrones[i].Reset();
        SyncExtraDrone(i);
    }
    if (!environment.empty())
    {
        colliders[TestCoverIndex].enabled = colliders[TestFloorIndex].enabled =
            colliders[TestBackWallIndex].enabled = false;
        colliders.insert(colliders.end(), environment.begin(), environment.end());
    }
    drone.Reset();
    if (droneMode)
    {
        colliders[PrimaryBodyIndex] = drone.Body();
        colliders[PrimaryWeaponIndex] = drone.Weapon();
    }
    if (!primaryEnabled)
    {
        colliders[PrimaryBodyIndex].enabled = colliders[PrimaryWeaponIndex].enabled = false;
    }
    ResetCombat(supplyPosition);
    midboss.Reset();
    phase = TurretPhase::Idle;
    phaseTime = doorProgress = 0;
    hitsTaken = blockedEnemyShots = 0;
    aimPoint = glm::vec3(0);
}

void TurretEncounter::SetTurrets(TurretType type, std::initializer_list<glm::vec3> positions, bool primary)
{
    primaryEnabled = primary;
    turrets.clear();
    for (const auto& position : positions)
    {
        const auto index = static_cast<ObjectId>(turrets.size());
        turrets.emplace_back().Configure(type, position, index);
    }
    Reset();
}

void TurretEncounter::SetTurrets(std::initializer_list<TurretPlacement> placements, bool primary)
{
    primaryEnabled = primary;
    turrets.clear();
    for (const auto& placement : placements)
    {
        const auto index = static_cast<ObjectId>(turrets.size());
        turrets.emplace_back().Configure(placement.type, placement.position, index);
    }
    Reset();
}

void TurretEncounter::SetExtraDrones(std::initializer_list<ExtraDronePlacement> placements)
{
    std::vector<DroneBehavior> configured;
    configured.reserve(placements.size());
    for (const auto& placement : placements)
    {
        DroneBehavior extra;
        if (!extra.SetSpawn(placement.position, placement.rotationYRadians,
                            placement.position.z + placement.patrolOffsetZ) ||
            !extra.SetScale(placement.scale))
        {
            return;
        }
        configured.push_back(extra);
    }
    extraDrones.swap(configured);
    Reset();
}

bool TurretEncounter::IsExtraDroneCollider(ObjectId id) const
{
    return id >= ExtraDroneBodyId && id - ExtraDroneBodyId < 2 * extraDrones.size();
}

int TurretEncounter::RemainingEnemies() const
{
    int count = (EnemyAlive() ? 1 : 0) + (midboss.Alive() ? 1 : 0);
    for (const auto& turret : turrets)
        if (turret.Alive()) ++count;
    for (const auto& extra : extraDrones)
        if (extra.Phase() != DronePhase::Destroyed) ++count;
    return count;
}

void TurretEncounter::SetEnvironment(const std::vector<Collider>& walls)
{
    environment = walls;
    Reset();
}

void TurretEncounter::SetExitDoors(ObjectId left, ObjectId right, float distance)
{
    leftDoor = left;
    rightDoor = right;
    doorDistance = distance;
    Reset();
}

void TurretEncounter::UpdateDoors(float gameDeltaTime)
{
    if (!leftDoor || !rightDoor || doorDistance <= 0)
    {
        return;
    }
    doorProgress = std::min(1.0f, doorProgress + gameDeltaTime);
    for (auto& box : colliders)
    {
        if (box.id != leftDoor && box.id != rightDoor)
        {
            continue;
        }
        for (const auto& original : environment)
        {
            if (original.id == box.id)
            {
                const float offset = (box.id == leftDoor ? -1 : 1) * doorDistance * doorProgress;
                box.minimum = original.minimum + glm::vec3(offset, 0, 0);
                box.maximum = original.maximum + glm::vec3(offset, 0, 0);
                break;
            }
        }
    }
}

bool TurretEncounter::SetDroneSpawn(const glm::vec3& point, float rotYRadians, float endZ)
{
    if (!drone.SetSpawn(point, rotYRadians, endZ))
    {
        return false;
    }
    Reset();
    return true;
}

bool TurretEncounter::SetSupply(const glm::vec3& point)
{
    if (!IsFinite(point))
    {
        return false;
    }
    supplyPosition = point;
    Reset();
    return true;
}

void TurretEncounter::SetDroneMode(bool enabled)
{
    droneMode = enabled;
    Reset();
}

const std::vector<Collider>& TurretEncounter::InteractionColliders(std::vector<Collider>&) const
{
    return colliders;
}

void TurretEncounter::ToggleCover()
{
    if (environment.empty() && (result == EncounterResult::Running || result == EncounterResult::Securing))
    {
        colliders[TestCoverIndex].enabled = !colliders[TestCoverIndex].enabled;
    }
}

void TurretEncounter::Update(float realDeltaTime, float gameDeltaTime, const glm::vec3& eye,
                             const glm::vec3& forward)
{
    if (!std::isfinite(realDeltaTime) || !std::isfinite(gameDeltaTime) ||
        realDeltaTime < 0 || gameDeltaTime < 0 || !IsFinite(eye))
    {
        return;
    }

    UpdateBulletEffects(gameDeltaTime);
    if (result == EncounterResult::Failed) return;
    UpdateShotCooldown(realDeltaTime);
    damageFlash = std::max(0.0f, damageFlash - gameDeltaTime);
    if (gameDeltaTime == 0)
    {
        return;
    }
    if (result == EncounterResult::Cleared)
    {
        UpdateDoors(gameDeltaTime);
        return;
    }

    // 1. 적의 조준과 이동을 갱신한다. 실제 발사는 피격 판정이 끝난 뒤에 한다.
    const bool droneWantsToShoot = UpdateDrone(gameDeltaTime, eye);
    std::vector<std::size_t> readyTurrets;
    for (std::size_t i = 0; i < turrets.size(); ++i)
    {
        if (turrets[i].Update(gameDeltaTime, eye, environment))
            readyTurrets.push_back(i);
        SyncTurret(i);
    }

    std::vector<std::size_t> readyDrones;
    if (!extraDrones.empty())
    {
        std::vector<Collider> walls;
        for (const auto& box : colliders)
        {
            if (box.enabled && (box.kind == HitKind::Wall || box.kind == HitKind::Armor))
                walls.push_back(box);
        }
        for (std::size_t i = 0; i < extraDrones.size(); ++i)
        {
            if (extraDrones[i].Update(gameDeltaTime, eye, walls))
                readyDrones.push_back(i);
            SyncExtraDrone(i);
        }
    }

    // 2. 플레이어 탄환의 명중을 먼저 처리한다. 방금 파괴된 적은 발사할 수 없다.
    const int laserDamage = midboss.Update(gameDeltaTime, eye, environment);
    UpdatePlayerBullets(gameDeltaTime);
    UpdateEnemyBullets(gameDeltaTime, eye, forward);
    if (laserDamage > 0 && health > 0 && midboss.Alive())
    {
        health = std::max(0, health - laserDamage);
        hitsTaken += laserDamage / Midboss::DamagePerTick;
        damageFlash = .3f;
    }
    if (health == 0)
    {
        FailEncounter();
        return;
    }

    // 3. 살아 있는 적의 발사 요청만 실행한다.
    for (const auto index : readyTurrets)
    {
        const auto& turret = turrets[index];
        if (turret.Armed())
            SpawnEnemyBullet(turret.Muzzle(), turret.AimPoint(), turret.ShotSource(), BulletStyle::Turret);
    }

    for (const auto index : readyDrones)
    {
        const auto& extra = extraDrones[index];
        if (extra.Armed())
            SpawnEnemyBullet(extra.Muzzle(), extra.AimPoint(),
                             ExtraDroneShotId + static_cast<ObjectId>(index), BulletStyle::Drone);
    }

    // 4. 적이 모두 죽어도 날아오는 적탄이 없어질 때까지 구역을 열지 않는다.
    if (RemainingEnemies() == 0)
    {
        phase = TurretPhase::Destroyed;
        if (enemyFlights.empty())
        {
            result = EncounterResult::Cleared;
            playerFlights.clear();
        }
        return;
    }
    if (!EnemyAlive())
    {
        return;
    }
    if (!colliders[PrimaryWeaponIndex].enabled)
    {
        phase = TurretPhase::Disarmed;
        return;
    }

    if (droneMode)
    {
        if (droneWantsToShoot && drone.Armed())
        {
            SpawnEnemyBullet(drone.Muzzle(), drone.AimPoint(), 900, BulletStyle::Drone);
        }
        return;
    }
    UpdatePrimaryTurret(gameDeltaTime, eye);
}

bool TurretEncounter::UpdateDrone(float gameDeltaTime, const glm::vec3& eye)
{
    bool wantsToShoot = false;
    if (droneMode && EnemyAlive())
    {
        std::vector<Collider> walls;
        for (const auto& box : colliders)
        {
            if (box.enabled && (box.kind == HitKind::Wall || box.kind == HitKind::Armor))
            {
                walls.push_back(box);
            }
        }
        wantsToShoot = drone.Update(gameDeltaTime, eye, walls);
        colliders[PrimaryBodyIndex] = drone.Body();
        colliders[PrimaryWeaponIndex] = drone.Weapon();
        switch (drone.Phase())
        {
        case DronePhase::Warning:
            phase = TurretPhase::Warning;
            break;
        case DronePhase::Waiting:
            phase = TurretPhase::Cooldown;
            break;
        case DronePhase::Disarmed:
            phase = TurretPhase::Disarmed;
            break;
        default:
            phase = TurretPhase::Idle;
            break;
        }
    }

    return wantsToShoot;
}

void TurretEncounter::SyncTurret(std::size_t index)
{
    const auto first = FirstTurretIndex + FixedTurret::TargetCount * index;
    const auto targets = turrets[index].Targets();
    for (std::size_t i = 0; i < targets.size(); ++i)
        colliders[first + i] = targets[i];
}

std::size_t TurretEncounter::ExtraDroneColliderIndex(std::size_t index) const
{
    return FirstTurretIndex + FixedTurret::TargetCount * turrets.size() + 2 * index;
}

void TurretEncounter::SyncExtraDrone(std::size_t index)
{
    const auto first = ExtraDroneColliderIndex(index);
    const auto bodyId = ExtraDroneBodyId + static_cast<ObjectId>(2 * index);
    colliders[first] = extraDrones[index].Body();
    colliders[first].id = bodyId;
    colliders[first + 1] = extraDrones[index].Weapon();
    colliders[first + 1].id = bodyId + 1;
}

void TurretEncounter::UpdatePlayerBullets(float gameDeltaTime)
{
    // Player shots never collide with the shooter's body. One impact per bullet.
    for (auto& flight : playerFlights)
    {
        Hit hit, bossHit;
        const auto start = flight.bullet.position;
        const auto end = start + flight.bullet.velocity * gameDeltaTime;
        const bool hitBoss = midboss.FindHit(start, end, bossHit);
        if (AdvanceFlight(flight, gameDeltaTime, colliders, hit, hitBoss ? &bossHit : nullptr))
        {
            ShatterBullet(flight.bullet, false);
            if (hit.objectId == Midboss::CoreId && hit.kind == HitKind::Body)
            {
                midboss.HitCore();
                inventory.Break(flight.bullet.sourceWeaponId);
                continue;
            }
            for (std::size_t i = 0; i < turrets.size(); ++i)
            {
                auto& turret = turrets[i];
                const bool bodyHit = hit.objectId == turret.BodyTarget();
                if (!bodyHit && hit.objectId != turret.WeaponTarget())
                    continue;
                inventory.Break(flight.bullet.sourceWeaponId);
                // 포신을 직접 맞혔으면 회수 가능한 무기를 남기지 않는다.
                if (bodyHit && turret.Armed())
                    inventory.SpawnDrop(turret.Position() + glm::vec3(0, 0.12f, 0.9f));
                turret.Destroy();
                SyncTurret(i);
                break;
            }
            for (std::size_t i = 0; i < extraDrones.size(); ++i)
            {
                auto& extra = extraDrones[i];
                const auto bodyId = ExtraDroneBodyId + static_cast<ObjectId>(2 * i);
                if (hit.objectId == bodyId + 1)
                {
                    extra.Disarm();
                }
                else if (hit.objectId == bodyId)
                {
                    inventory.Break(flight.bullet.sourceWeaponId);
                    if (extra.Armed())
                    {
                        const auto drop = extra.Muzzle();
                        inventory.SpawnDrop({drop.x, 0.12f, drop.z});
                    }
                    extra.Destroy();
                }
                else continue;
                SyncExtraDrone(i);
                break;
            }
            if (hit.objectId == turretWeaponId)
            {
                colliders[PrimaryWeaponIndex].enabled = false;
                phase = TurretPhase::Disarmed;
                if (droneMode)
                {
                    drone.Disarm();
                }
            }
            else if (hit.objectId == turretId)
            {
                inventory.Break(flight.bullet.sourceWeaponId);
                if (colliders[PrimaryWeaponIndex].enabled)
                {
                    const auto drop = droneMode ? drone.Muzzle() : glm::vec3(0.85f, 0.12f, -9.3f);
                    inventory.SpawnDrop({drop.x, 0.12f, drop.z});
                }
                if (droneMode)
                {
                    drone.Destroy();
                }
                colliders[PrimaryWeaponIndex].enabled = false;
                colliders[PrimaryBodyIndex].enabled = false;
                phase = TurretPhase::Destroyed;
                if (RemainingEnemies() == 0)
                {
                    result = EncounterResult::Securing;
                }
            }
        }
    }
    RemoveExpiredFlights(playerFlights);
    if (RemainingEnemies() == 0)
    {
        result = EncounterResult::Securing;
    }
}

std::vector<Collider> TurretEncounter::CreateEnemyTargets(const glm::vec3& eye,
                                                          const glm::vec3& forward) const
{
    auto targets = colliders;
    targets.push_back(PlayerBodyCollider(playerId, eye));
    targets.push_back(inventory.HeldCollider(eye, forward));
    return targets;
}

void TurretEncounter::UpdateEnemyBullets(float gameDeltaTime, const glm::vec3& eye, const glm::vec3& forward)
{
    auto enemyColliders = CreateEnemyTargets(eye, forward);
    const auto heldWeaponIndex = colliders.size() + 1;
    for (auto& flight : enemyFlights)
    {
        // 적탄은 자신을 발사한 적의 몸과 총을 무시한다.
        const bool fromPrimary = flight.bullet.sourceWeaponId == 900;
        enemyColliders[PrimaryBodyIndex].enabled = !fromPrimary && colliders[PrimaryBodyIndex].enabled;
        enemyColliders[PrimaryWeaponIndex].enabled = !fromPrimary && colliders[PrimaryWeaponIndex].enabled;
        for (std::size_t i = 0; i < turrets.size(); ++i)
        {
            const bool otherTurret = turrets[i].ShotSource() != flight.bullet.sourceWeaponId;
            const auto first = FirstTurretIndex + FixedTurret::TargetCount * i;
            for (auto slot = first; slot < first + FixedTurret::TargetCount; ++slot)
                enemyColliders[slot].enabled = otherTurret && colliders[slot].enabled;
        }
        for (std::size_t i = 0; i < extraDrones.size(); ++i)
        {
            const bool otherDrone =
                flight.bullet.sourceWeaponId != ExtraDroneShotId + static_cast<ObjectId>(i);
            const auto first = ExtraDroneColliderIndex(i);
            enemyColliders[first].enabled = otherDrone && colliders[first].enabled;
            enemyColliders[first + 1].enabled = otherDrone && colliders[first + 1].enabled;
        }
        enemyColliders[heldWeaponIndex] = inventory.HeldCollider(eye, forward);
        Hit hit;
        if (AdvanceFlight(flight, gameDeltaTime, enemyColliders, hit))
        {
            ShatterBullet(flight.bullet, true);
            if (hit.kind == HitKind::Body && hit.objectId == playerId)
            {
                health = std::max(0, health - 25);
                ++hitsTaken;
                damageFlash = 0.3f;
            }
            else if (hit.kind == HitKind::Weapon)
            {
                inventory.Break(hit.objectId);
                ++blockedEnemyShots;
            }
            else
            {
                ++blockedEnemyShots;
            }
        }
        if (health == 0)
        {
            break;
        }
    }
    RemoveExpiredFlights(enemyFlights);
}

void TurretEncounter::FailEncounter()
{
    result = EncounterResult::Failed;
    playerFlights.clear();
    enemyFlights.clear();
}

void TurretEncounter::SpawnEnemyBullet(const glm::vec3& origin, const glm::vec3& target,
                                       ObjectId sourceWeapon, BulletStyle bulletStyle)
{
    Bullet bullet;
    if (!MakeBullet(origin, target - origin, 7.0f, sourceWeapon, bullet))
    {
        return;
    }
    enemyFlights.push_back({bullet, 0, bulletStyle});
    ++enemyShots;
}

void TurretEncounter::UpdatePrimaryTurret(float gameDeltaTime, const glm::vec3& eye)
{
    auto enemyColliders = CreateEnemyTargets(eye, {0, 0, -1});
    enemyColliders[PrimaryBodyIndex].enabled = false;
    enemyColliders[PrimaryWeaponIndex].enabled = false;
    enemyColliders.back().enabled = false; // 플레이어가 든 총 때문에 적의 시야가 막히지 않게 한다.
    Hit sight;
    const bool visible = FindFirstHit(muzzle, eye, enemyColliders, sight) && sight.objectId == playerId;
    if (phase == TurretPhase::Cooldown)
    {
        phaseTime -= gameDeltaTime;
        if (phaseTime <= 0)
        {
            phase = TurretPhase::Idle;
        }
        return;
    }
    if (!visible)
    {
        phase = TurretPhase::Idle;
        phaseTime = 0;
        return;
    }
    if (phase == TurretPhase::Idle)
    {
        phase = TurretPhase::Warning;
        phaseTime = 1.0f;
        aimPoint = eye;
        return;
    }
    phaseTime -= gameDeltaTime;
    if (phaseTime <= 0)
    {
        SpawnEnemyBullet(muzzle, aimPoint, 900, BulletStyle::Turret);
        phase = TurretPhase::Cooldown;
        phaseTime = 1.2f;
    }
}

} // namespace aegis::combat
