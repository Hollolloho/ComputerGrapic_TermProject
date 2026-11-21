// 보스의 공격 패턴, 약점 명중과 승패를 관리한다.
#include "boss_encounter.h"
#include "main/GameMath.h"
#include <algorithm>
#include <cmath>
#include <glm/geometric.hpp>

namespace aegis::combat
{
BossEncounter::BossEncounter() : walls(boss_asset::walls.begin(), boss_asset::walls.end())
{
    Reset();
}

void BossEncounter::Reset()
{
    ResetCombat(boss_asset::supplyInteraction);
    phase = BossPhase::Warning;
    aimPoint = FiringRoot() + glm::vec3(0, 1.05f, 0);
    timer = 1.2f;
    armorOpen = 0;
    coreHits = blockedShots = 0;
    pose = MakeBossPose(0, aimPoint);
}

const std::vector<Collider>& BossEncounter::InteractionColliders(std::vector<Collider>&) const
{
    return walls;
}

void BossEncounter::Shoot(const glm::vec3& eye)
{
    for (int side = 0; side < 2; ++side)
    {
        const auto muzzle = BossMuzzle(pose, side);
        // The physical cabinets/room block firing toward a covered player.
        if (IsBlocked(muzzle, eye - glm::vec3(0, 0.6f, 0), walls))
        {
            continue;
        }
        const auto node = side ? boss_asset::muzzle_R : boss_asset::muzzle_L;
        Bullet shot;
        if (MakeBullet(muzzle, glm::vec3(pose.nodes[node][2]), 7, 23000 + side, shot))
        {
            enemyFlights.push_back({shot, 0});
            ++enemyShots;
        }
    }
}

void BossEncounter::AdvancePattern(float dt, const glm::vec3& eye)
{
    if (phase == BossPhase::Defeated)
    {
        return;
    }
    if (phase == BossPhase::Warning)
    {
        aimPoint = eye - glm::vec3(0, 0.6f, 0);
    }
    timer -= dt;
    if (phase == BossPhase::Opening)
    {
        armorOpen = std::clamp(1 - timer / 0.75f, 0.0f, 1.0f);
    }
    if (phase == BossPhase::Closing)
    {
        armorOpen = std::clamp(timer / 0.6f, 0.0f, 1.0f);
    }
    pose = MakeBossPose(armorOpen, aimPoint);
    if (timer > 0)
    {
        return;
    }
    switch (phase)
    {
    case BossPhase::Warning:
        phase = BossPhase::Attack;
        timer = 0.35f;
        Shoot(eye);
        break;
    case BossPhase::Attack:
        phase = BossPhase::Opening;
        timer = 0.75f;
        break;
    case BossPhase::Opening:
        phase = BossPhase::Exposed;
        timer = 3;
        armorOpen = 1;
        break;
    case BossPhase::Exposed:
        phase = BossPhase::Closing;
        timer = 0.6f;
        break;
    case BossPhase::Closing:
        phase = BossPhase::Recovery;
        timer = 1.2f;
        armorOpen = 0;
        break;
    case BossPhase::Recovery:
        phase = BossPhase::Warning;
        timer = 1.2f;
        break;
    case BossPhase::Defeated:
        break;
    }
    pose = MakeBossPose(armorOpen, aimPoint);
}

void BossEncounter::Update(float realDeltaTime, float gameDeltaTime, const glm::vec3& eye,
                           const glm::vec3& forward)
{
    if (result == EncounterResult::Failed || !IsFinite(eye) || !std::isfinite(realDeltaTime) ||
        !std::isfinite(gameDeltaTime) || realDeltaTime < 0 || gameDeltaTime < 0)
    {
        return;
    }
    UpdateShotCooldown(realDeltaTime);
    damageFlash = std::max(0.0f, damageFlash - gameDeltaTime);
    if (gameDeltaTime == 0 || result == EncounterResult::Cleared)
    {
        return;
    }
    if (result == EncounterResult::Running)
    {
        AdvancePattern(gameDeltaTime, eye);
    }
    UpdatePlayerBullets(gameDeltaTime);
    UpdateEnemyBullets(gameDeltaTime, eye, forward);
    if (health == 0)
    {
        result = EncounterResult::Failed;
        playerFlights.clear();
        enemyFlights.clear();
        return;
    }
    if (result == EncounterResult::Securing && enemyFlights.empty())
    {
        result = EncounterResult::Cleared;
        playerFlights.clear();
    }
}

void BossEncounter::UpdatePlayerBullets(float gameDeltaTime)
{
    for (auto& flight : playerFlights)
    {
        auto& bullet = flight.bullet;
        const auto end = bullet.position + bullet.velocity * gameDeltaTime;
        Hit hit, bossHit;
        bool found = FindFirstHit(bullet.position, end, walls, hit);
        if (FindBossHit(bullet.position, end, pose, phase == BossPhase::Exposed, bossHit) &&
            (!found || bossHit.fraction < hit.fraction))
        {
            hit = bossHit;
            found = true;
        }
        if (bullet.active && found)
        {
            bullet.position = hit.position;
            bullet.active = false;
            if (hit.kind == HitKind::Body && phase == BossPhase::Exposed)
            {
                ++coreHits;
                inventory.Break(bullet.sourceWeaponId);
                phase = coreHits >= 3 ? BossPhase::Defeated : BossPhase::Closing;
                timer = 0.6f; // Core is disabled immediately, even while panels travel shut.
                if (coreHits >= 3)
                {
                    result = EncounterResult::Securing;
                }
            }
            else
            {
                ++blockedShots;
            }
        }
        else if (bullet.active)
        {
            bullet.position = end;
        }
        flight.age += gameDeltaTime;
    }
    RemoveExpiredFlights(playerFlights);
}

void BossEncounter::UpdateEnemyBullets(float gameDeltaTime, const glm::vec3& eye, const glm::vec3& forward)
{
    for (auto& flight : enemyFlights)
    {
        auto boxes = walls;
        boxes.push_back({24000, HitKind::Body, eye + glm::vec3(-0.3f, -1.65f, -0.3f),
                         eye + glm::vec3(0.3f, 0.15f, 0.3f)});
        boxes.push_back(inventory.HeldCollider(eye, forward));
        Hit hit;
        if (AdvanceBullet(flight.bullet, gameDeltaTime, boxes, hit))
        {
            if (hit.kind == HitKind::Body && hit.objectId == 24000)
            {
                health = std::max(0, health - 25);
                damageFlash = 0.3f;
            }
            else if (hit.kind == HitKind::Weapon)
            {
                inventory.Break(hit.objectId);
            }
        }
        flight.age += gameDeltaTime;
    }
    RemoveExpiredFlights(enemyFlights);
}

} // namespace aegis::combat
