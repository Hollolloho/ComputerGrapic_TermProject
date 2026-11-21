// 개발 중 상태를 창 제목에 표시한다. 실제 게임 진행을 바꾸지 않는다.
#include "combat_scene.h"
#include <string>

namespace aegis
{
std::string CombatScene::StatusTitle(bool paused) const
{
    if (coreMode)
    {
        const char* phase = core.Phase() == combat::MagneticPhase::Warning ? "MAGNETIC WARNING"
                            : core.Phase() == combat::MagneticPhase::Pulse ? "MAGNETIC PULSE"
                            : core.CoreAlive() ? "CORE ACTIVE" : "CORE DESTROYED";
        return std::string("AEGIS CORE | ") + (paused ? "PAUSED | " : "") + phase +
               " | Core HP: " + std::to_string(core.CoreHealth()) + "/100 | HP: " +
               std::to_string(core.Health()) + " | Guards: " +
               std::to_string(core.RemainingEnemies() - (core.CoreAlive() ? 1 : 0)) +
               " | Shots: " + std::to_string(core.PlayerShots()) +
               " | Supplies: " + std::to_string(Inventory().Supplies());
    }
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
               " | E refill / R reset / Esc menu";
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
    std::string turretStatus;
    for (const auto& turret : encounter.Turrets())
    {
        turretStatus += " | Turret: ";
        if (!turret.Alive()) turretStatus += "DESTROYED";
        else if (!turret.Armed()) turretStatus += "DISARMED";
        else if (turret.Phase() == combat::TurretPhase::Warning) turretStatus += "WARNING";
        else turretStatus += "ACTIVE";
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
           " | Enemies: " + std::to_string(encounter.RemainingEnemies()) + turretStatus +
           " | Hostiles: " + std::to_string(encounter.EnemyFlights().size()) +
           " | Cover: " + (encounter.CoverEnabled() ? "ON" : "OFF") +
           " | Analysis: " + (CanAnalyze() ? "READY" : "LOCKED") +
           " | Entries: " + std::to_string(analysis.Entries()) +
           " | Exits: " + std::to_string(analysis.Exits()) +
           " | Door: " + std::to_string(static_cast<int>(DoorProgress() * 100)) + weaponStatus +
           " | G drop / E use / R retry";
}

} // namespace aegis
