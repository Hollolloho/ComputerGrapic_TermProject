// 전투 구역별 배치, 시점과 이동 충돌 범위를 설정한다.
#include "combat_scene.h"
#include "combat_map/corridor_layout.generated.h"
#include "combat_map/terminal_layout.generated.h"
#include <limits>

namespace aegis
{
combat::EncounterResult CombatScene::BattleResult() const
{
    return activeBattle->Result();
}

void CombatScene::SetCorridorMode(bool enabled)
{
    activeBattle = &encounter;
    combatMapMode = false;
    maintenanceMode = approachMode = false;
    encounter.SetDroneScale(1);
    encounter.SetTurrets(combat::TurretType::SingleC, {});
    encounter.SetDroneSpawn({-0.65f, 1.8f, -10}, 0, -10.f);
    encounter.SetSupply(combat::WeaponInventory::SupplyPosition());
    corridorMode = enabled;
    std::vector<combat::Collider> walls;
    if (enabled)
    {
        walls.assign(corridor_asset::walls.begin(), corridor_asset::walls.end());
        walls.push_back(terminal_asset::collider);
    }
    encounter.SetEnvironment(walls);
    encounter.SetExitDoors(enabled ? 10010 : 0, enabled ? 10011 : 0,
                           enabled ? corridor_asset::doorTravel : 0);
    encounter.SetDroneMode(enabled);
    Reset();
}

void CombatScene::BeginCombatMap()
{
    activeBattle = &encounter;
    combatMapMode = true;
    corridorMode = true;
    encounter.SetExitDoors(0, 0, 0);
    encounter.SetDroneMode(true);
}

void CombatScene::SetCombatMapMode(bool enabled)
{
    if (!enabled)
    {
        SetCorridorMode(true);
        return;
    }
    BeginCombatMap();
    maintenanceMode = approachMode = false;
    encounter.SetDroneScale(1);
    std::vector<combat::Collider> walls(combat_map_asset::walls.begin(), combat_map_asset::walls.end());
    walls.push_back(TerminalCollider());
    encounter.SetEnvironment(walls);
    // 순찰 끝의 엄폐물(Cover_A02) 위로 무기까지 통과할 높이를 확보한다.
    glm::vec3 droneStart = combat_map_asset::droneSpawn;
    droneStart.y += 0.4f;
    encounter.SetDroneSpawn(droneStart, glm::radians(combat_map_asset::droneRotYDegrees), -14.40f);
    encounter.SetSupply(combat_map_asset::supply);
    encounter.SetTurrets(combat::TurretType::SingleC, {{-1.55f, 0, -15.9f}});
    Reset();
}

// 정비 구간에는 드론과 단포신 두 대, 다음 복도에는 쌍포신 한 대를 배치한다.
void CombatScene::SetMaintenanceMode(bool coreApproach)
{
    BeginCombatMap();
    maintenanceMode = true;
    approachMode = coreApproach;
    if (approachMode)
        encounter.SetTurrets(combat::TurretType::Twin, {{8.6f, 0, -45.7f}}, false);
    else
        encounter.SetTurrets(combat::TurretType::SingleC, {{3.85f, 0, -36.8f}, {3.75f, 0, -40.8f}});

    std::vector<combat::Collider> walls(combat_map_asset::maintenanceWalls.begin(),
                                        combat_map_asset::maintenanceWalls.end());
    walls.push_back(TerminalCollider());
    encounter.SetEnvironment(walls);

    const auto& placement = combat_map_asset::friendDrones[1];
    encounter.SetDroneSpawn(placement.position, glm::radians(placement.rotYDegrees), placement.position.z);
    encounter.SetDroneScale(placement.scale);
    encounter.SetSupply(SupplyPosition());
    Reset();
}

void CombatScene::SetDroneMode(bool enabled)
{
    encounter.SetDroneMode(enabled);
    Reset();
}

void CombatScene::SetBossMode(bool enabled)
{
    activeBattle = &encounter;
    if (enabled)
    {
        activeBattle = &boss;
        combatMapMode = false;
        maintenanceMode = approachMode = false;
    }
    Reset();
}

void CombatScene::SetCoreMode()
{
    activeBattle = &core;
    maintenanceMode = approachMode = false;
    combatMapMode = corridorMode = true;
    Reset();
}

glm::vec3 CombatScene::CameraRoot() const
{
    if (IsCoreMode()) return combat::CoreEncounter::PlayerRoot();
    if (IsBossMode())
    {
        if (bossCovered)
        {
            return combat::BossEncounter::CoverRoot();
        }
        return combat::BossEncounter::FiringRoot();
    }
    if (approachMode) return {7.8f, 0, -40.6f};
    if (maintenanceMode)
    {
        return combat_map_asset::maintenanceStart;
    }
    if (combatMapMode)
    {
        return combat_map_asset::playerStart;
    }
    return glm::vec3(0);
}

bool CombatScene::SetPlayerView(const PlayerView& view)
{
    if (analysis.Active() || DeathStarted())
    {
        return false;
    }
    if (!externalView.Set(view))
    {
        return false;
    }
    lookHeld = false;
    return true;
}

PlayerView CombatScene::CurrentPlayerView() const
{
    const PlayerView fallback{camera.EyePosition(), camera.ViewDirection(), {0, 1, 0}, 60};
    return externalView.Selected(fallback);
}

void CombatScene::ToggleBossCover()
{
    bossCovered = !bossCovered;
    const auto target = bossCovered ? boss_asset::supplyInteraction : boss.CoreCenter();
    const auto d = glm::normalize(target - (CameraRoot() + glm::vec3(0, 1.65f, 0)));
    rotY = glm::degrees(std::atan2(-d.x, -d.z));
    rotX = glm::degrees(std::asin(d.y));
    camera.SetPose(CameraRoot(), rotY, rotX);
    lookHeld = false;
}

std::vector<combat::Collider> CombatScene::PlayerObstacles() const
{
    if (IsCoreMode()) return core.PlayerObstacles();
    if (IsBossMode())
    {
        std::vector<combat::Collider> walls(boss_asset::walls.begin(), boss_asset::walls.end());
        glm::vec3 low(std::numeric_limits<float>::max()), high(std::numeric_limits<float>::lowest());
        for (int i = 0; i < boss_asset::partCount; ++i)
        {
            const auto& part = boss_asset::parts[i];
            for (int corner = 0; corner < 8; ++corner)
            {
                const auto point =
                    glm::vec3(boss.Pose().parts[i] * glm::vec4(corner & 1 ? part.high.x : part.low.x,
                                                               corner & 2 ? part.high.y : part.low.y,
                                                               corner & 4 ? part.high.z : part.low.z, 1));
                low = glm::min(low, point);
                high = glm::max(high, point);
            }
        }
        walls.push_back({29000, combat::HitKind::Armor, low, high});
        return walls;
    }
    if (combatMapMode)
    {
        std::vector<combat::Collider> walls(combat_map_asset::playerWalls.begin(),
                                            combat_map_asset::playerWalls.end());
        walls.push_back(TerminalCollider());
        for (const auto& turret : encounter.Turrets())
        {
            if (!turret.Alive()) continue;
            auto body = turret.Body();
            body.kind = combat::HitKind::Armor;
            walls.push_back(body);
        }
        return walls;
    }
    return encounter.Colliders();
}

} // namespace aegis
