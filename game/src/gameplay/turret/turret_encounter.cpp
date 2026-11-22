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
const glm::vec3 muzzle{0.85f, 1.65f, -9.05f};

} // namespace

TurretEncounter::TurretEncounter()
{
    Reset();
}

void TurretEncounter::Reset()
{
    // 각 포탑은 본체, 첫 포신, 둘째 포신 순서로 충돌 상자를 둔다. Single_C는 둘째 포신이 비활성이다.
    colliders = {{{turretId, HitKind::Body, {-0.6f, 0, -10.4f}, {0.6f, 2.25f, -9.5f}},
                  {200, HitKind::Wall, {-0.85f, 0, -6.3f}, {0.85f, 2.5f, -5.8f}, false},
                  {300, HitKind::Wall, {-6, -0.2f, -14}, {6, 0, 3}},
                  {301, HitKind::Wall, {-6, 0, -13}, {6, 4, -12.8f}},
                  {turretWeaponId, HitKind::Weapon, {0.65f, 1.45f, -9.5f}, {1.05f, 1.85f, -9.05f}}}};
    for (auto& turret : turrets)
    {
        turret.Reset();
        colliders.push_back(turret.Body());
        colliders.push_back(turret.Weapon());
        colliders.push_back(turret.Weapon(1));
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

int TurretEncounter::RemainingEnemies() const
{
    int count = EnemyAlive() ? 1 : 0;
    for (const auto& turret : turrets)
        if (turret.Alive()) ++count;
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

    UpdateShards(impactShards, gameDeltaTime);
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

    // 2. 플레이어 탄환의 명중을 먼저 처리한다. 방금 파괴된 적은 발사할 수 없다.
    UpdatePlayerBullets(gameDeltaTime);
    UpdateEnemyBullets(gameDeltaTime, eye, forward);
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
            SpawnEnemyBullet(turret.Muzzle(), turret.AimPoint(), turret.ShotSource());
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
            SpawnEnemyBullet(drone.Muzzle(), drone.AimPoint(), 900);
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
    const auto first = FirstTurretIndex + 3 * index;
    colliders[first] = turrets[index].Body();
    colliders[first + 1] = turrets[index].Weapon();
    colliders[first + 2] = turrets[index].Weapon(1);
}

void TurretEncounter::UpdatePlayerBullets(float gameDeltaTime)
{
    // Player shots never collide with the shooter's body. One impact per bullet.
    for (auto& flight : playerFlights)
    {
        Hit hit;
        if (AdvanceBullet(flight.bullet, gameDeltaTime, colliders, hit))
        {
            ShatterBullet(flight.bullet, false);
            for (std::size_t i = 0; i < turrets.size(); ++i)
            {
                auto& turret = turrets[i];
                if (hit.objectId == turret.WeaponTarget())
                    turret.Disarm();
                else if (hit.objectId == turret.BodyTarget())
                {
                    inventory.Break(flight.bullet.sourceWeaponId);
                    if (turret.Armed())
                        inventory.SpawnDrop(turret.Position() + glm::vec3(0, 0.12f, 0.9f));
                    turret.Destroy();
                }
                else continue;
                SyncTurret(i);
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
        flight.age += gameDeltaTime;
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
    targets.push_back(
        {playerId, HitKind::Body, eye + glm::vec3(-0.3f, -1.65f, -0.3f), eye + glm::vec3(0.3f, 0.15f, 0.3f)});
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
            const auto first = FirstTurretIndex + 3 * i;
            for (auto slot = first; slot < first + 3; ++slot)
                enemyColliders[slot].enabled = otherTurret && colliders[slot].enabled;
        }
        enemyColliders[heldWeaponIndex] = inventory.HeldCollider(eye, forward);
        Hit hit;
        if (AdvanceBullet(flight.bullet, gameDeltaTime, enemyColliders, hit))
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
        flight.age += gameDeltaTime;
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
                                       ObjectId sourceWeapon)
{
    Bullet bullet;
    if (!MakeBullet(origin, target - origin, 7.0f, sourceWeapon, bullet))
    {
        return;
    }
    enemyFlights.push_back({bullet, 0});
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
        Bullet bullet;
        if (MakeBullet(muzzle, aimPoint - muzzle, 7.0f, 900, bullet))
        {
            enemyFlights.push_back({bullet, 0});
            ++enemyShots;
        }
        phase = TurretPhase::Cooldown;
        phaseTime = 1.2f;
    }
}

} // namespace aegis::combat
