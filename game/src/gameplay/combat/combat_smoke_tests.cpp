// 자동 실행 검사 전용 파일. 일반 플레이의 입력이나 적 AI를 작성하는 곳이 아니다.
#include "combat_scene.h"
#include "combat_map/corridor_layout.generated.h"
#include "combat_map/terminal_layout.generated.h"
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>
#include <fstream>
#include <iostream>

namespace aegis
{
// 자동 실행 검사 전용. 실제 전투 로직은 combat_scene.cpp에서 처리한다.
void CombatScene::PlayerViewSmokeFrame(int frame)
{
    const auto aim = [&](const glm::vec3& eye, const glm::vec3& target, float fov = 45)
    { return SetPlayerView({eye, target - eye, {0, 1, 0}, fov}); };
    if (frame == 1)
    {
        encounter.SetSupportTurret(false); // Isolate camera/interaction contracts from encounter composition.
    }
    if (frame == 1)
    {
        aim(combat_map_asset::playerStart + glm::vec3(1, 1.65f, -1), combat_map_asset::droneSpawn);
    }
    if (frame == 5)
    {
        Fire(PlayerForward());
    }
    if (frame == 80)
    {
        smokeViewShot = HasExternalPlayerView() && encounter.Result() == combat::EncounterResult::Cleared &&
                        encounter.PlayerShots() == 1 && Inventory().Breaks() == 1;
        aim({0, 1.65f, 2}, SupplyPosition());
        const bool far = !Interact(PlayerForward());
        aim({-1.2f, 1.65f, -2.4f}, SupplyPosition());
        smokeViewSupply = far && Interact(PlayerForward()) && Inventory().Armed();
        encounter.DropWeapon(PlayerEye(), {1, 0, 0});
        const auto drop = Inventory().Drops().back().position;
        aim(drop + glm::vec3(0, 1.65f, 5), drop);
        const bool farDrop = !Interact(PlayerForward());
        aim({-1.2f, 1.65f, -2.4f}, drop);
        smokeViewPickup = farDrop && Interact(PlayerForward()) && Inventory().Pickups() == 1;
    }
    if (frame == 100)
    {
        const auto target = terminal_asset::interactionPoint + TerminalPosition() - terminal_asset::position;
        aim({0.1f, 1.65f, -2}, target, 40);
        smokeViewSnapshot = CurrentPlayerView();
        const bool opened = Interact(PlayerForward());
        const bool rejected = !SetPlayerView({{99, 99, 99}, {0, 0, -1}, {0, 1, 0}, 60});
        ClearPlayerView();
        smokeViewAnalysis = opened && AnalysisActive() && rejected && HasExternalPlayerView();
    }
    if (frame == 120)
    {
        analysis.Close();
        smokeViewRestored = CurrentPlayerView().eye == smokeViewSnapshot.eye &&
                            CurrentPlayerView().forward == smokeViewSnapshot.forward &&
                            CurrentPlayerView().verticalFovDegrees == smokeViewSnapshot.verticalFovDegrees;
    }
    if (frame == 130)
    {
        Reset();
        smokeViewReset = !HasExternalPlayerView() && encounter.Health() == 100 &&
                         CurrentPlayerView().eye == combat_map_asset::playerStart + glm::vec3(0, 1.65f, 0);
    }
    if (frame == 135)
    {
        aim({2, 1.65f, -4}, combat_map_asset::droneSpawn);
    }
    if (frame == 300)
    {
        smokeViewAim = encounter.EnemyShots() == 1 &&
                       glm::length(encounter.DroneState().AimPoint() - glm::vec3(2, 1.65f, -4)) < 0.001f;
    }
    if (frame == 310)
    {
        aim({-0.5f, 1.65f, -4}, encounter.DroneState().Position());
    }
    if (frame == 400)
    {
        smokeViewDodge = encounter.EnemyShots() == 1 && encounter.Health() == 100;
    }
    if (frame == 410)
    {
        Reset();
        smokeViewDone = !HasExternalPlayerView();
    }
}

bool CombatScene::PlayerViewSmokePassed() const
{
    std::cout << "External view checks: shot=" << smokeViewShot << " supply=" << smokeViewSupply
              << " pickup=" << smokeViewPickup << " analysis=" << smokeViewAnalysis
              << " restore=" << smokeViewRestored << " reset=" << smokeViewReset << " aim=" << smokeViewAim
              << " dodge=" << smokeViewDodge << " done=" << smokeViewDone << '\n';
    return smokeViewShot && smokeViewSupply && smokeViewPickup && smokeViewAnalysis && smokeViewRestored &&
           smokeViewReset && smokeViewAim && smokeViewDodge && smokeViewDone;
}

void CombatScene::CampaignSmokeFrame(int frame)
{
    if (coreMode)
    {
        // 실제 사격·보급 경로로 호위 네 기와 코어를 정리한다.
        forceNormal = true;
        if (core.Result() != combat::EncounterResult::Running || !core.PlayerFlights().empty())
        {
            return;
        }
        const auto entrance = combat::CoreEncounter::PlayerRoot() + glm::vec3(0, 1.65f, 0);
        if (!Inventory().Armed())
        {
            const auto direction = combat::CoreEncounter::SupplyPosition() - entrance;
            if (SetPlayerView({entrance, direction, {0, 1, 0}, 60}))
            {
                Interact(direction);
            }
            return;
        }

        const auto shootGuard = [&](const combat::Collider& body)
        {
            const auto target = (body.minimum + body.maximum) * .5f;
            const glm::vec3 offsets[] = {{0, 0, 2}, {2, 0, 0}, {-2, 0, 0}, {0, 0, -2}};
            for (const auto& offset : offsets)
            {
                const auto eye = target + offset;
                if (combat::FindFirstHit(eye, target, combat_map_asset::playerWalls))
                {
                    continue;
                }
                const auto direction = target - eye;
                if (SetPlayerView({eye, direction, {0, 1, 0}, 60}))
                {
                    Fire(direction);
                    return true;
                }
            }
            return false;
        };
        for (const auto& drone : core.Drones())
        {
            if (drone.Phase() != combat::DronePhase::Destroyed)
            {
                shootGuard(drone.Body());
                return;
            }
        }
        for (const auto& turret : core.Turrets())
        {
            if (turret.Alive())
            {
                shootGuard(turret.Body());
                return;
            }
        }
        // 입구는 반경 6m 밖이다. 자기장을 피하면서 열 발을 명중시킨다.
        const auto direction = core.CoreCenter() - entrance;
        if (SetPlayerView({entrance, direction, {0, 1, 0}, 60}))
        {
            Fire(direction);
        }
        return;
    }
    if (bossMode)
    {
        if (frame == 1)
        {
            Fire(boss.CoreCenter() - PlayerEye());
            ToggleBossCover();
        }
        if (boss.Result() != combat::EncounterResult::Running)
        {
            return;
        }
        if (boss.Phase() == combat::BossPhase::Exposed && Inventory().Armed())
        {
            if (bossCovered)
            {
                ToggleBossCover();
            }
            if (boss.PlayerFlights().empty())
            {
                Fire(boss.CoreCenter() - PlayerEye());
            }
        }
        else
        {
            if (!bossCovered)
            {
                ToggleBossCover();
            }
            if (!Inventory().Armed())
            {
                Interact(boss_asset::supplyInteraction - PlayerEye());
            }
        }
    }
    else
    {
        if (frame == 1)
        {
            Fire(encounter.DroneState().Position() - PlayerEye());
        }
        if (!encounter.EnemyAlive() && encounter.RemainingEnemies() > 0 && encounter.PlayerFlights().empty())
        {
            if (!Inventory().Armed())
            {
                Interact(SupplyPosition() - PlayerEye());
            }
            if (Inventory().Armed())
            {
                Fire(encounter.SupportTurret().Position() + glm::vec3(0, 0.9f, 0) - PlayerEye());
            }
        }
    }
}

void CombatScene::SetSmokeFrame(int frame)
{
    if (combatMapMode)
    {
        smokeMapDeath = smokeMapDeath || DeathPlaying();
        smokeMapBoth = smokeMapBoth || encounter.RemainingEnemies() == 2;
        smokeMapTurretShot =
            smokeMapTurretShot ||
            std::any_of(encounter.EnemyFlights().begin(), encounter.EnemyFlights().end(),
                        [](const combat::Flight& f)
                        { return f.projectile.sourceWeaponId == combat::FixedTurret::ShotId; });
        if (frame == 1)
        {
            slowPreview = true;
        }
        if (frame == 180)
        {
            smokeMapSlow =
                encounter.Health() == 100 && encounter.EnemyShots() == 0 && time.Scale() < 0.051 &&
                glm::length(encounter.DroneState().Position() - combat_map_asset::droneSpawn) < 0.2f;
            slowPreview = false;
        }
        if (frame == 1200)
        {
            smokeFailureSeen = encounter.Result() == combat::EncounterResult::Failed;
            Reset();
        }
        if (frame == 1201)
        {
            Fire(glm::vec3(-1.9f, 1, -7.45f) - PlayerEye());
        }
        if (frame == 1239)
        {
            smokeMapCoverBlocked =
                encounter.EnemyAlive() && Inventory().Armed() && encounter.PlayerFlights().empty();
        }
        if (frame == 1240)
        {
            Fire(encounter.DroneState().Position() - PlayerEye());
        }
        if (frame == 1310)
        {
            smokeMapLocked = !encounter.EnemyAlive() && encounter.RemainingEnemies() == 1 && !CanAnalyze() &&
                             encounter.Result() == combat::EncounterResult::Running;
        }
        if (frame == 1320)
        {
            Interact(SupplyPosition() - PlayerEye());
        }
        if (frame == 1321)
        {
            Fire(encounter.SupportTurret().Position() + glm::vec3(0, 0.9f, 0) - PlayerEye());
        }
        if (frame == 1450)
        {
            Interact(SupplyPosition() - PlayerEye());
        }
        return;
    }
    if (bossMode)
    {
        if (frame == 900)
        {
            smokeBossFailureSeen = boss.Result() == combat::EncounterResult::Failed;
            Reset();
        }
        if (frame == 901)
        {
            Fire(boss.CoreCenter() - PlayerEye());
            ToggleBossCover();
        }
        if (frame > 901 && boss.Result() == combat::EncounterResult::Running)
        {
            if (boss.Phase() == combat::BossPhase::Exposed && Inventory().Armed())
            {
                if (bossCovered)
                {
                    ToggleBossCover();
                }
                if (boss.PlayerFlights().empty())
                {
                    Fire(boss.CoreCenter() - PlayerEye());
                }
            }
            else
            {
                if (!bossCovered)
                {
                    ToggleBossCover();
                }
                if (!Inventory().Armed())
                {
                    Interact(boss_asset::supplyInteraction - PlayerEye());
                }
            }
        }
        return;
    }
    if (IsDroneMode())
    {
        smokePatrolSeen = smokePatrolSeen || encounter.DroneState().Position().x > -0.5f;
        smokeWarningSeen = smokeWarningSeen || encounter.DroneState().Phase() == combat::DronePhase::Warning;
        smokeShotSeen = smokeShotSeen || encounter.EnemyShots() > 0;
        if (frame == 900)
        {
            smokeFailureSeen = encounter.Result() == combat::EncounterResult::Failed;
            Reset();
        }
        smokeSecuringSeen = smokeSecuringSeen || encounter.Result() == combat::EncounterResult::Securing;
        if (frame == 1150)
        {
            smokeDoorSnapshot = DoorProgress();
            Interact(terminal_asset::interactionPoint - PlayerEye());
        }
        if (frame == 1160)
        {
            smokeAnalysisSeen = analysis.Active();
            smokeWorldFrozen = DoorProgress() == smokeDoorSnapshot && smokeDoorSnapshot < 1;
        }
        if (frame == 1190)
        {
            analysis.Close();
        }
        if (frame == 1191)
        {
            smokeAnalysisReturned = !analysis.Active() && analysis.Entries() > 0 && analysis.Exits() > 0;
        }
        if (frame == 1060)
        {
            Fire(encounter.DroneState().Position() - PlayerEye());
        }
        return;
    }
    if (turretMode)
    {
        if (frame == 660)
        {
            smokeFailureSeen = encounter.Result() == combat::EncounterResult::Failed;
            Reset();
        }
        if (frame == 661)
        {
            Fire(PlayerForward());
        }
        return;
    }
    if (frame == 1)
    {
        Fire({0, 0, -1});
    }
    if (frame == 45)
    {
        range.DropWeapon(PlayerEye(), PlayerForward());
    }
    if (frame == 50 && !range.Inventory().Drops().empty())
    {
        Interact(range.Inventory().Drops()[0].position - PlayerEye());
    }
    if (frame == 70)
    {
        range.ToggleCover();
        Fire({0, 0, -1});
    }
    if (frame == 130 || frame == 200)
    {
        Interact(combat::WeaponInventory::SupplyPosition() - PlayerEye());
    }
    if (frame == 140)
    {
        Fire(glm::vec3(-2.1f, 1.6f, -9.8f) - PlayerEye());
    }
    if (frame == 210)
    {
        Fire(glm::vec3(2.1f, 1.6f, -9.8f) - PlayerEye());
    }
}

bool CombatScene::SmokePassed() const
{
    if (feedback.BreakBursts() != Inventory().Breaks())
    {
        return false;
    }
    if (combatMapMode)
    {
        std::cout << "Mixed map checks: both=" << smokeMapBoth << " turretShot=" << smokeMapTurretShot
                  << " locked=" << smokeMapLocked << " slow=" << smokeMapSlow
                  << " cover=" << smokeMapCoverBlocked << " failure=" << smokeFailureSeen
                  << " remaining=" << encounter.RemainingEnemies() << " hp=" << encounter.Health()
                  << " breaks=" << Inventory().Breaks() << '\n';
        return smokeMapDeath && destruction.Bursts() == 2 && smokeMapBoth && smokeMapTurretShot &&
               smokeMapLocked && smokeMapSlow && smokeMapCoverBlocked && smokeFailureSeen &&
               encounter.Result() == combat::EncounterResult::Cleared && encounter.Health() > 0 &&
               encounter.RemainingEnemies() == 0 && encounter.PlayerShots() == 3 && Inventory().Armed() &&
               Inventory().Breaks() == 2 && Inventory().Supplies() == 2 && encounter.EnemyFlights().empty();
    }
    if (bossMode)
    {
        return smokeBossFailureSeen && boss.Result() == combat::EncounterResult::Cleared &&
               boss.CoreHits() == 3 && boss.Health() == 100 && boss.PlayerShots() == 4 &&
               boss.BlockedShots() >= 1 && Inventory().Breaks() == 3 && Inventory().Supplies() == 2 &&
               !Inventory().Armed() && weapon.ShotCount() == boss.PlayerShots() &&
               boss.EnemyFlights().empty();
    }
    if (IsDroneMode() && (!smokePatrolSeen || !smokeWarningSeen || !smokeShotSeen || !smokeSecuringSeen ||
                          !smokeAnalysisSeen || !smokeAnalysisReturned || !smokeWorldFrozen))
    {
        return false;
    }
    if (turretMode)
    {
        return smokeFailureSeen && encounter.Result() == combat::EncounterResult::Cleared &&
               encounter.Health() == (IsDroneMode() ? 75 : 100) && encounter.PlayerFlights().empty() &&
               encounter.EnemyFlights().empty() && weapon.ShotCount() == encounter.PlayerShots() &&
               !Inventory().Armed() && Inventory().Breaks() == 1 && Inventory().Drops().size() == 1 &&
               encounter.CanAnalyze() && (!corridorMode || encounter.DoorProgress() >= 1);
    }
    return range.RemainingTargets() == 0 && range.Shots() == 4 && range.BlockedShots() == 1 &&
           range.Flights().empty() && weapon.ShotCount() == range.Shots() && !Inventory().Armed() &&
           Inventory().Breaks() == 3 && Inventory().Supplies() == 2 && Inventory().Pickups() == 1 &&
           Inventory().Drops().size() == 3;
}

bool CombatScene::SaveFrame(const std::filesystem::path& path, int width, int height) const
{
    std::vector<unsigned char> pixels(static_cast<std::size_t>(width) * height * 3);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadBuffer(GL_BACK);
    glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
    glPixelStorei(GL_PACK_ALIGNMENT, 4);
    std::ofstream output(path, std::ios::binary);
    output << "P6\n" << width << ' ' << height << "\n255\n";
    for (int row = height - 1; row >= 0; --row)
    {
        output.write(reinterpret_cast<const char*>(pixels.data() + static_cast<std::size_t>(row) * width * 3),
                     width * 3);
    }
    return static_cast<bool>(output);
}

} // namespace aegis
