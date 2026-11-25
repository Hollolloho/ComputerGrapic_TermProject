// 전투 구역별 배치, 시점과 이동 충돌 범위를 설정한다.
#include "combat_scene.h"
#include "combat_map/corridor_layout.generated.h"
#include "combat_map/interlock_layout.h"
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
    interlockMode = false;
    maintenanceMode = false;
    encounter.SetExtraDrones({});
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
    interlockMode = false;
    maintenanceMode = false;
    encounter.SetExtraDrones({});
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

void CombatScene::SetInterlockMode()
{
    BeginCombatMap();
    interlockMode = true;

    std::vector<combat::Collider> walls;
    for (const auto& wall : combat_map_asset::playerWalls)
    {
        if (wall.minimum.z <= interlock_asset::entranceZ && wall.maximum.z >= interlock_asset::exitZ)
            walls.push_back(wall);
    }
    walls.push_back(TerminalCollider());
    encounter.SetEnvironment(walls);
    encounter.SetDroneSpawn({0.8f, 2.3f, -28.f}, 0.f, -25.5f);
    encounter.SetDroneScale(0.8f);
    encounter.SetTurrets(combat::TurretType::SingleC, {{-3.8f, 0.f, -29.3f}, {3.4f, 0.f, -26.2f}});
    encounter.SetSupply(SupplyPosition());
    Reset();
}

// 정비 복도 전체에서 드론 세 대와 단포신 두 대, 쌍포신 한 대를 함께 상대한다.
void CombatScene::SetMaintenanceMode()
{
    BeginCombatMap();
    maintenanceMode = true;
    encounter.SetTurrets({
        {combat::TurretType::SingleC, {6.55f, 0, -46.25f}},
        {combat::TurretType::SingleC, {9.0f, 0, -46.25f}},
        {combat::TurretType::Twin, {8.8f, 0, -38.7f}}
    });

    std::vector<combat::Collider> walls(combat_map_asset::maintenanceWalls.begin(),
                                        combat_map_asset::maintenanceWalls.end());
    walls.push_back(TerminalCollider());
    encounter.SetEnvironment(walls);

    const auto& placement = combat_map_asset::friendDrones[1];
    encounter.SetDroneSpawn(placement.position, glm::radians(placement.rotYDegrees), placement.position.z);
    encounter.SetDroneScale(placement.scale);
    encounter.SetExtraDrones({
        {{3.8f, 2.3f, -37.1f}, 0.f, 0.8f},
        {{8.9f, 2.3f, -43.2f}, 0.f, 0.8f}
    });
    encounter.SetSupply(SupplyPosition());
    Reset();
}

void CombatScene::SetDroneMode(bool enabled)
{
    if (!enabled) encounter.SetExtraDrones({});
    encounter.SetDroneMode(enabled);
    Reset();
}

void CombatScene::SetBossMode(bool enabled)
{
    encounter.SetExtraDrones({});
    activeBattle = &encounter;
    if (enabled)
    {
        activeBattle = &boss;
        combatMapMode = false;
        interlockMode = false;
        maintenanceMode = false;
    }
    Reset();
}

void CombatScene::SetCoreMode()
{
    encounter.SetExtraDrones({});
    activeBattle = &core;
    interlockMode = false;
    maintenanceMode = false;
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
    if (interlockMode)
    {
        return interlock_asset::playerStart;
    }
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
    const PlayerView fallback{camera.EyePosition(), camera.ViewDirection(), {0, 1, 0}};
    return externalView.Selected(fallback);
}

void CombatScene::ToggleBossCover()
{
    bossCovered = !bossCovered;
    const auto target = bossCovered ? boss_asset::supplyInteraction : boss.CoreCenter();
    const auto d = glm::normalize(target - (CameraRoot() + glm::vec3(0, player::EyeHeight, 0)));
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

const wchar_t* CombatScene::NextAreaDirection() const
{
    if (maintenanceMode)
        return L"코어실로";
    if (interlockMode || !combatMapMode)
        return L"정비 복도로";
    return L"사각방으로";
}

} // namespace aegis
