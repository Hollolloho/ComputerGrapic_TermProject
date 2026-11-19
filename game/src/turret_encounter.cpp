// 드론·포탑 전투의 상태를 관리한다. Update에서 처리 순서를 확인할 수 있다.
#include "turret_encounter.h"

#include <algorithm>
#include <cmath>

namespace aegis::combat
{
namespace
{
constexpr ObjectId turretId = 500;
constexpr ObjectId playerId = 600;
constexpr ObjectId turretWeaponId = 501;
const glm::vec3 muzzle{0.85f, 1.65f, -9.05f};

bool Finite(const glm::vec3& p)
{
    return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z);
}

void RemoveExpired(std::vector<Flight>& flights)
{
    std::erase_if(flights,
                  [](const Flight& flight) { return !flight.projectile.active || flight.age >= 4.0f; });
}
} // namespace

TurretEncounter::TurretEncounter()
{
    Reset();
}

void TurretEncounter::Reset()
{
    colliders = {{{turretId, HitKind::Body, {-0.6f, 0, -10.4f}, {0.6f, 2.25f, -9.5f}},
                  {200, HitKind::Wall, {-0.85f, 0, -6.3f}, {0.85f, 2.5f, -5.8f}, false},
                  {300, HitKind::Wall, {-6, -0.2f, -14}, {6, 0, 3}},
                  {301, HitKind::Wall, {-6, 0, -13}, {6, 4, -12.8f}},
                  {turretWeaponId, HitKind::Weapon, {0.65f, 1.45f, -9.5f}, {1.05f, 1.85f, -9.05f}}}};
    support.Reset();
    if (supportEnabled)
    {
        colliders.push_back(support.Body());
        colliders.push_back(support.Weapon());
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
    inventory.Reset();
    inventory.SetSupplyPosition(supplyPosition);
    playerFlights.clear();
    enemyFlights.clear();
    result = EncounterResult::Running;
    phase = TurretPhase::Idle;
    phaseTime = playerCooldown = damageFlash = doorProgress = 0;
    health = 100;
    playerShots = enemyShots = hitsTaken = blockedEnemyShots = 0;
    aimPoint = glm::vec3(0);
}

void TurretEncounter::SetSupportTurret(bool enabled)
{
    supportEnabled = enabled;
    Reset();
}

void TurretEncounter::SetEnvironment(std::span<const Collider> walls)
{
    environment.assign(walls.begin(), walls.end());
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
        const auto original = std::find_if(environment.begin(), environment.end(),
                                           [&](const Collider& c) { return c.id == box.id; });
        if (original == environment.end())
        {
            continue;
        }
        const float offset = (box.id == leftDoor ? -1 : 1) * doorDistance * doorProgress;
        box.minimum = original->minimum + glm::vec3(offset, 0, 0);
        box.maximum = original->maximum + glm::vec3(offset, 0, 0);
    }
}

bool TurretEncounter::SetDroneSpawn(const glm::vec3& point, float yawRadians, float patrolCenterX)
{
    if (!drone.SetSpawn(point, yawRadians, patrolCenterX))
    {
        return false;
    }
    Reset();
    return true;
}

bool TurretEncounter::SetSupply(const glm::vec3& point)
{
    if (!Finite(point))
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

bool TurretEncounter::Fire(const glm::vec3& eye, const glm::vec3& direction)
{
    if (!inventory.Armed() || result != EncounterResult::Running || playerCooldown > 0 ||
        playerFlights.size() >= 64)
    {
        return false;
    }
    auto projectile = MakeProjectile(eye, direction, 12.0f, inventory.EquippedId());
    if (!projectile)
    {
        return false;
    }
    playerFlights.push_back({*projectile, 0});
    playerCooldown = 0.25f;
    ++playerShots;
    return true;
}

bool TurretEncounter::DropWeapon(const glm::vec3& eye, const glm::vec3& direction)
{
    return result != EncounterResult::Failed && inventory.Drop(eye, direction);
}

bool TurretEncounter::Interact(const glm::vec3& eye, const glm::vec3& direction)
{
    return result != EncounterResult::Failed && inventory.Interact(eye, direction, colliders);
}

WeaponInteraction TurretEncounter::InteractionHint(const glm::vec3& eye, const glm::vec3& direction) const
{
    return result == EncounterResult::Failed ? WeaponInteraction::None
                                             : inventory.Hint(eye, direction, colliders);
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
    if (result == EncounterResult::Failed || !std::isfinite(realDeltaTime) || !std::isfinite(gameDeltaTime) ||
        realDeltaTime < 0 || gameDeltaTime < 0 || !Finite(eye))
    {
        return;
    }

    playerCooldown = std::max(0.0f, playerCooldown - realDeltaTime);
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
    const bool turretWantsToShoot = UpdateSupportTurret(gameDeltaTime, eye);

    // 2. 플레이어 탄환의 명중을 먼저 처리한다. 방금 파괴된 적은 발사할 수 없다.
    UpdatePlayerBullets(gameDeltaTime);
    UpdateEnemyBullets(gameDeltaTime, eye, forward);
    if (health == 0)
    {
        FailEncounter();
        return;
    }

    // 3. 살아 있는 적의 발사 요청만 실행한다.
    if (supportEnabled && turretWantsToShoot && support.Armed())
    {
        SpawnEnemyBullet(support.Muzzle(), support.AimPoint(), FixedTurret::ShotId);
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

bool TurretEncounter::UpdateSupportTurret(float gameDeltaTime, const glm::vec3& eye)
{
    bool wantsToShoot = false;
    if (supportEnabled)
    {
        std::vector<Collider> walls;
        for (const auto& box : colliders)
        {
            if (box.enabled && (box.kind == HitKind::Wall || box.kind == HitKind::Armor))
            {
                walls.push_back(box);
            }
        }
        wantsToShoot = support.Update(gameDeltaTime, eye, walls);
        colliders[SupportBodyIndex] = support.Body();
        colliders[SupportWeaponIndex] = support.Weapon();
    }
    return wantsToShoot;
}

void TurretEncounter::UpdatePlayerBullets(float gameDeltaTime)
{
    // Player shots never collide with the shooter's body. One impact per bullet.
    for (auto& flight : playerFlights)
    {
        if (auto hit = AdvanceProjectile(flight.projectile, gameDeltaTime, colliders))
        {
            if (supportEnabled && hit->objectId == FixedTurret::WeaponId)
            {
                support.Disarm();
                colliders[SupportWeaponIndex] = support.Weapon();
            }
            else if (supportEnabled && hit->objectId == FixedTurret::BodyId)
            {
                inventory.Break(flight.projectile.sourceWeaponId);
                if (support.Armed())
                {
                    inventory.SpawnDrop(support.Position() + glm::vec3(0, 0.12f, 0.9f));
                }
                support.Destroy();
                colliders[SupportBodyIndex] = support.Body();
                colliders[SupportWeaponIndex] = support.Weapon();
            }
            else if (hit->objectId == turretWeaponId)
            {
                colliders[PrimaryWeaponIndex].enabled = false;
                phase = TurretPhase::Disarmed;
                if (droneMode)
                {
                    drone.Disarm();
                }
            }
            else if (hit->objectId == turretId)
            {
                inventory.Break(flight.projectile.sourceWeaponId);
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
    RemoveExpired(playerFlights);
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
        const bool fromSupport = flight.projectile.sourceWeaponId == FixedTurret::ShotId;
        enemyColliders[PrimaryBodyIndex].enabled = fromSupport && colliders[PrimaryBodyIndex].enabled;
        enemyColliders[PrimaryWeaponIndex].enabled = fromSupport && colliders[PrimaryWeaponIndex].enabled;
        if (supportEnabled)
        {
            enemyColliders[SupportBodyIndex].enabled = !fromSupport && colliders[SupportBodyIndex].enabled;
            enemyColliders[SupportWeaponIndex].enabled =
                !fromSupport && colliders[SupportWeaponIndex].enabled;
        }
        enemyColliders[heldWeaponIndex] = inventory.HeldCollider(eye, forward);
        const auto hit = AdvanceProjectile(flight.projectile, gameDeltaTime, enemyColliders);
        if (hit)
        {
            if (hit->kind == HitKind::Body && hit->objectId == playerId)
            {
                health = std::max(0, health - 25);
                ++hitsTaken;
                damageFlash = 0.3f;
            }
            else if (hit->kind == HitKind::Weapon)
            {
                inventory.Break(hit->objectId);
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
    RemoveExpired(enemyFlights);
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
    const auto bullet = MakeProjectile(origin, target - origin, 7.0f, sourceWeapon);
    if (!bullet)
    {
        return;
    }
    enemyFlights.push_back({*bullet, 0});
    ++enemyShots;
}

void TurretEncounter::UpdatePrimaryTurret(float gameDeltaTime, const glm::vec3& eye)
{
    auto enemyColliders = CreateEnemyTargets(eye, {0, 0, -1});
    enemyColliders[PrimaryBodyIndex].enabled = false;
    enemyColliders[PrimaryWeaponIndex].enabled = false;
    enemyColliders.back().enabled = false; // 플레이어가 든 총 때문에 적의 시야가 막히지 않게 한다.
    const auto sight = FindFirstHit(muzzle, eye, enemyColliders);
    const bool visible = sight && sight->objectId == playerId;
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
        if (auto projectile = MakeProjectile(muzzle, aimPoint - muzzle, 7.0f, 900))
        {
            enemyFlights.push_back({*projectile, 0});
            ++enemyShots;
        }
        phase = TurretPhase::Cooldown;
        phaseTime = 1.2f;
    }
}

} // namespace aegis::combat
