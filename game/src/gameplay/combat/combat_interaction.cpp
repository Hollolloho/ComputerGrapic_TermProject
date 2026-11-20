// 전투 장면에서 사격, 무기 획득과 분석 단말 상호작용을 처리한다.
#include "combat_scene.h"
#include "combat_map/terminal_layout.generated.h"

namespace aegis
{
bool CombatScene::Fire(const glm::vec3& direction)
{
    if (!time.CanFire())
    {
        return false;
    }
    const bool fired = ActiveEncounter()->Fire(PlayerEye(), direction);
    if (fired)
    {
        time.RequestShot();
        weapon.OnShot();
    }
    return fired;
}

const combat::WeaponInventory& CombatScene::Inventory() const
{
    return ActiveEncounter()->Inventory();
}

bool CombatScene::Interact(const glm::vec3& direction)
{
    if (TerminalTarget(direction))
    {
        return analysis.Open(CanAnalyze(), TerminalCollider(), PlayerEye(), direction, encounter.Colliders());
    }
    return ActiveEncounter()->Interact(PlayerEye(), direction);
}

glm::vec3 CombatScene::TerminalPosition() const
{
    if (maintenanceMode)
    {
        return combat_map_asset::maintenanceTerminal;
    }
    return combatMapMode ? combat_map_asset::terminal : terminal_asset::position;
}

combat::Collider CombatScene::TerminalCollider() const
{
    auto box = terminal_asset::collider;
    const auto offset = TerminalPosition() - terminal_asset::position;
    box.minimum += offset;
    box.maximum += offset;
    return box;
}

glm::vec3 CombatScene::SupplyPosition() const
{
    if (coreMode) return combat::CoreEncounter::SupplyPosition();
    if (maintenanceMode)
    {
        return combat_map_asset::maintenanceSupply;
    }
    return combatMapMode ? combat_map_asset::supply : combat::WeaponInventory::SupplyPosition();
}

bool CombatScene::TerminalTarget() const
{
    return TerminalTarget(PlayerForward());
}

bool CombatScene::TerminalTarget(const glm::vec3& direction) const
{
    if (!corridorMode || bossMode || coreMode)
    {
        return false;
    }
    return analysis.IsFirstTarget(
        TerminalCollider(), PlayerEye(), direction, encounter.Colliders(),
        Inventory().InteractionDistance(PlayerEye(), direction, encounter.Colliders()));
}

combat::WeaponInteraction CombatScene::InteractionHint() const
{
    if (TerminalTarget())
    {
        return combat::WeaponInteraction::None;
    }
    return ActiveEncounter()->InteractionHint(PlayerEye(), PlayerForward());
}

} // namespace aegis
