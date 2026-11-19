// 개발 중 상태를 창 제목에 표시한다. 실제 게임 진행을 바꾸지 않는다.
#include "combat_scene.h"
#include <string>

namespace aegis
{
std::string CombatScene::StatusTitle(bool paused) const
{
    if (bossMode)
    {
        return std::string("AEGIS BOSS | ") + (paused ? "PAUSED | " : "") +
               (boss.Result() == combat::EncounterResult::Failed     ? "FAILED"
                : boss.Result() == combat::EncounterResult::Cleared  ? "VICTORY"
                : boss.Result() == combat::EncounterResult::Securing ? "SECURING"
                : boss.Phase() == combat::BossPhase::Exposed         ? "CORE OPEN"
                : boss.Phase() == combat::BossPhase::Warning         ? "WARNING"
                                                                     : "ARMOR CLOSED") +
               " | Core: " + std::to_string(boss.CoreHits()) + "/3 | HP: " + std::to_string(boss.Health()) +
               " | Shots: " + std::to_string(boss.PlayerShots()) +
               " | Blocked: " + std::to_string(boss.BlockedShots()) +
               " | Weapon: " + (Inventory().Armed() ? "READY" : "EMPTY") +
               " | Supplies: " + std::to_string(Inventory().Supplies()) +
               " | Cover: " + (bossCovered ? "ON" : "OFF") +
               " | B test position / E refill / R reset / F4 corridor";
    }
    if (analysis.Active())
    {
        return "AEGIS ANALYSIS | World: PAUSED | Enter: " + std::to_string(analysis.Entries()) +
               " | Exit: " + std::to_string(analysis.Exits()) + " | E or Esc return";
    }
    const std::string weaponStatus = std::string(" | Weapon: ") + (Inventory().Armed() ? "READY" : "EMPTY") +
                                     " | Breaks: " + std::to_string(Inventory().Breaks()) +
                                     " | Supplies: " + std::to_string(Inventory().Supplies()) +
                                     " | Pickups: " + std::to_string(Inventory().Pickups()) +
                                     " | Drops: " + std::to_string(Inventory().Drops().size());
    if (!turretMode)
    {
        return std::string("AEGIS RANGE | ") + (paused ? "PAUSED | " : "") +
               "Targets: " + std::to_string(range.RemainingTargets()) +
               "/3 | Shots: " + std::to_string(range.Shots()) +
               " | Blocked: " + std::to_string(range.BlockedShots()) +
               " | Cover: " + (range.CoverEnabled() ? "ON" : "OFF") + weaponStatus +
               " | G drop / E pickup or supply / T turret / R reset";
    }
    const char* state = encounter.Result() == combat::EncounterResult::Failed     ? "FAILED"
                        : encounter.Result() == combat::EncounterResult::Cleared  ? "CLEARED"
                        : encounter.Result() == combat::EncounterResult::Securing ? "SECURING"
                        : encounter.Phase() == combat::TurretPhase::Warning       ? "WARNING"
                        : encounter.Phase() == combat::TurretPhase::Cooldown      ? "COOLDOWN"
                        : encounter.Phase() == combat::TurretPhase::Disarmed      ? "DISARMED"
                        : IsDroneMode()                                           ? "PATROL"
                                                                                  : "READY";
    return std::string(IsDroneMode()  ? "AEGIS DRONE | "
                       : corridorMode ? "AEGIS CORRIDOR | "
                                      : "AEGIS TURRET | ") +
           (paused ? "PAUSED | " : "") + state + " | HP: " + std::to_string(encounter.Health()) +
           " | Hits: " + std::to_string(encounter.HitsTaken()) +
           " | EnemyShots: " + std::to_string(encounter.EnemyShots()) +
           " | PlayerShots: " + std::to_string(encounter.PlayerShots()) +
           " | Enemies: " + std::to_string(encounter.RemainingEnemies()) + " | Turret: " +
           (encounter.HasSupportTurret() ? !encounter.SupportTurret().Alive() ? "DESTROYED"
                                           : encounter.SupportTurret().Phase() == combat::TurretPhase::Warning
                                               ? "WARNING"
                                           : !encounter.SupportTurret().Armed() ? "DISARMED"
                                                                                : "ACTIVE"
                                         : "OFF") +
           " | Hostiles: " + std::to_string(encounter.EnemyFlights().size()) +
           " | Cover: " + (encounter.CoverEnabled() ? "ON" : "OFF") +
           " | Analysis: " + (CanAnalyze() ? "READY" : "LOCKED") +
           " | Entries: " + std::to_string(analysis.Entries()) +
           " | Exits: " + std::to_string(analysis.Exits()) +
           " | Door: " + std::to_string(static_cast<int>(DoorProgress() * 100)) + weaponStatus +
           (corridorMode ? " | F3 enemy / G drop / E use / R retry / F2 range"
                         : " | G drop / E pickup or supply / R retry / T range");
}

} // namespace aegis
