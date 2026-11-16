#include "combat.h"
#include "combat_range.h"
#include "turret_encounter.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <vector>
#include <glm/geometric.hpp>

using namespace aegis::combat;

int main()
{
    int failures = 0;
    int checks = 0;
    const auto check = [&](bool result, const char* name) {
        ++checks;
        std::cout << (result ? "PASS " : "FAIL ") << name << '\n';
        if (!result) ++failures;
    };
    const auto close = [](float a, float b) { return std::abs(a - b) < 0.0001f; };
    const glm::vec3 origin{0, 0, 0};
    std::vector<Collider> boxes{
        {2, HitKind::Body, {-1,-1,-9}, {1,1,-8}},
        {1, HitKind::Wall, {-1,-1,-4.01f}, {1,1,-4}}};
    auto bullet = *MakeProjectile(origin, {0,0,-5}, 100, 77);
    check(close(glm::length(bullet.velocity), 100), "direction normalized to requested speed");
    auto hit = AdvanceProjectile(bullet, 0.1f, boxes);
    check(hit && hit->objectId == 1 && close(hit->position.z, -4),
        "thin wall blocks target even when crossed in one frame");
    check(!bullet.active && bullet.sourceWeaponId == 77, "impact deactivates bullet and preserves source weapon ID");
    check(!AdvanceProjectile(bullet, 0.1f, boxes), "same bullet cannot produce a second impact");
    std::reverse(boxes.begin(), boxes.end());
    hit = FindFirstHit(origin, {0,0,-10}, boxes);
    check(hit && hit->objectId == 1, "collision result independent of collider order");
    boxes[0].enabled = false;
    hit = FindFirstHit(origin, {0,0,-10}, boxes);
    check(hit && hit->objectId == 2, "disabled wall allows body hit");
    check(!FindFirstHit(origin, {0,0,-2}, boxes), "target beyond segment is not hit");
    check(!FindFirstHit({3,0,0}, {3,0,-20}, boxes), "parallel segment outside box misses");
    hit = FindFirstHit({0,0,-8.5f}, {0,0,-10}, boxes);
    check(hit && hit->fraction == 0, "starting inside target hits immediately");
    hit = FindFirstHit(origin, {0,0,-8}, boxes);
    check(hit && hit->fraction == 1, "exact segment endpoint counts as impact");
    bullet = *MakeProjectile(origin, {0,0,-1}, 10, 8);
    const std::span<const Collider> empty;
    AdvanceProjectile(bullet, 0, empty);
    check(bullet.position == origin && bullet.active, "pause does not advance projectile");
    AdvanceProjectile(bullet, 0.05f, empty);
    check(close(bullet.position.z, -0.5f), "projectile uses supplied slow game time");
    AdvanceProjectile(bullet, -1, empty);
    AdvanceProjectile(bullet, std::numeric_limits<float>::infinity(), empty);
    check(close(bullet.position.z, -0.5f), "negative and non-finite time are ignored");
    check(!MakeProjectile(origin, origin, 10, 1), "zero direction rejected");
    check(!MakeProjectile(origin, {0,0,-1}, -1, 1), "negative speed rejected");
    check(!MakeProjectile(origin, {std::numeric_limits<float>::quiet_NaN(),0,0}, 10, 1), "non-finite direction rejected");
    std::array<Collider, 2> overlap{{
        {4, HitKind::Body, {-1,-1,-5}, {1,1,-4}},
        {5, HitKind::Weapon, {-1,-1,-5}, {1,1,-4}}}};
    hit = FindFirstHit(origin, {0,0,-10}, overlap);
    check(hit && hit->kind == HitKind::Weapon, "overlapping weapon/body produces only weapon impact");
    overlap[0].kind = HitKind::Wall;
    hit = FindFirstHit(origin, {0,0,-10}, overlap);
    check(hit && hit->kind == HitKind::Wall, "wall wins exact-distance tie");
    overlap[0].minimum.z = 20;
    hit = FindFirstHit(origin, {0,0,-10}, overlap);
    check(hit && hit->objectId == 5, "invalid box bounds are ignored");
    bullet = *MakeProjectile(origin, {0,0,-1}, 10, 8);
    auto splitBullet = bullet;
    AdvanceProjectile(bullet, 0.1f, empty);
    for (int i = 0; i < 10; ++i) AdvanceProjectile(splitBullet, 0.01f, empty);
    check(close(bullet.position.z, splitBullet.position.z), "movement consistent across frame subdivisions");
    CombatRange range;
    const glm::vec3 eye{0,1.65f,0};
    check(range.Fire(eye,{0,0,-1}), "range accepts first shot");
    check(!range.Fire(eye,{0,0,-1}), "range prevents free rapid fire during cooldown");
    for (int i=0;i<60;++i) range.Update(1.0f/60,1.0f/60);
    check(range.RemainingTargets()==3 && range.BlockedShots()==1 && range.Flights().empty(),
        "range cover blocks centre target and removes projectile");
    range.ToggleCover();
    range.Fire(eye,{0,0,-1});
    for (int i=0;i<60;++i) range.Update(1.0f/60,1.0f/60);
    check(range.RemainingTargets()==2, "range centre target disappears after cover removed");
    range.Interact(eye,WeaponInventory::SupplyPosition()-eye);
    range.Fire(eye,glm::vec3(-2.1f,1.6f,-9.8f)-eye);
    for (int i=0;i<60;++i) range.Update(1.0f/60,1.0f/60);
    range.Interact(eye,WeaponInventory::SupplyPosition()-eye);
    range.Fire(eye,glm::vec3(2.1f,1.6f,-9.8f)-eye);
    for (int i=0;i<60;++i) range.Update(1.0f/60,1.0f/60);
    check(range.RemainingTargets()==0 && range.Shots()==4, "range can complete all three targets");
    range.Reset();
    check(range.RemainingTargets()==3 && range.Shots()==0 && range.BlockedShots()==0
        && range.CoverEnabled() && range.Flights().empty() && !range.LastHit(),
        "range restart restores all combat state");
    range.Fire(eye,{0,0,-1});
    range.Update(0,0);
    check(range.Flights().size()==1 && range.Flights()[0].projectile.position==eye,
        "paused range does not advance bullet");
    range.Reset();
    range.Fire(eye,{0,1,0});
    for (int i=0;i<300;++i) range.Update(1.0f/60,1.0f/60);
    check(range.Flights().empty(), "missed bullets expire");
    TurretEncounter battle;
    const auto tick=[&](int frames) {
        for(int frame=0;frame<frames;++frame) battle.Update(0.01f,0.01f,eye);
    };
    battle.Update(0.01f,0.01f,eye);
    check(battle.Phase()==TurretPhase::Warning && battle.EnemyShots()==0,
        "turret warns before first shot");
    tick(50);
    check(battle.EnemyShots()==0 && battle.Health()==100,"warning cannot damage player");
    tick(60);
    check(battle.EnemyShots()==1 && battle.Health()==100 && battle.EnemyFlights().size()==1,
        "turret launches visible bullet after warning, not instant damage");
    tick(150);
    check(battle.Health()==75 && battle.HitsTaken()==1,"enemy bullet applies one 25-point hit");
    const auto healthAfterHit=battle.Health();
    const auto shotsAfterHit=battle.EnemyShots();
    battle.Update(0,0,eye);
    check(battle.Health()==healthAfterHit && battle.EnemyShots()==shotsAfterHit,
        "paused encounter does not deal damage or advance turret");
    battle.Reset(); battle.ToggleCover(); tick(500);
    check(battle.EnemyShots()==0 && battle.Health()==100 && battle.Phase()==TurretPhase::Idle,
        "occluding wall prevents turret fire");
    battle.Reset(); tick(50); battle.ToggleCover(); tick(200);
    check(battle.EnemyShots()==0,"closing cover cancels warning");
    battle.ToggleCover(); tick(50);
    check(battle.EnemyShots()==0 && battle.Phase()==TurretPhase::Warning,
        "reacquiring player requires fresh warning");
    battle.Reset(); tick(110); battle.ToggleCover(); tick(200);
    check(battle.BlockedEnemyShots()==1 && battle.Health()==100 && battle.EnemyFlights().empty(),
        "cover stops in-flight enemy bullet without player damage");
    battle.Reset(); tick(1200);
    check(battle.Result()==EncounterResult::Failed && battle.Health()==0 && battle.HitsTaken()==4,
        "four hits cause failure without negative health");
    check(battle.PlayerFlights().empty() && battle.EnemyFlights().empty(),
        "failure removes all active projectiles");
    const int failedShots=battle.EnemyShots();
    check(!battle.Fire(eye,{0,0,-1}),"failed player cannot fire");
    battle.ToggleCover(); tick(300);
    check(battle.EnemyShots()==failedShots && battle.Health()==0 && !battle.CoverEnabled(),
        "failure freezes AI and gameplay until restart");
    battle.Reset();
    check(battle.Health()==100 && battle.Result()==EncounterResult::Running && battle.Phase()==TurretPhase::Idle
        && battle.EnemyShots()==0 && battle.PlayerShots()==0 && battle.HitsTaken()==0
        && battle.BlockedEnemyShots()==0 && !battle.CoverEnabled() && battle.Colliders()[0].enabled,
        "restart restores player, turret, cover, counters and phase");
    battle.Fire(eye,{0,0,-1}); tick(100);
    check(battle.Result()==EncounterResult::Cleared && battle.Health()==100 && !battle.Colliders()[0].enabled,
        "player bullet destroys turret and clears encounter");
    check(battle.PlayerFlights().empty() && battle.EnemyFlights().empty() && !battle.Fire(eye,{0,0,-1}),
        "clear stops projectiles and blocks new shots");
    battle.Reset();
    for(int i=0;i<500;++i) battle.Update(0.01f,0.001f,eye);
    check(battle.EnemyShots()==0 && battle.Phase()==TurretPhase::Warning,
        "turret warning uses game time instead of real time");
    battle.Update(-1,0.1f,eye);
    battle.Update(0.1f,std::numeric_limits<float>::infinity(),eye);
    check(battle.EnemyShots()==0 && battle.Health()==100,"invalid encounter delta is ignored");
    battle.Reset(); battle.ToggleCover(); battle.Fire(eye,{0,0,-1}); tick(150);
    check(battle.Result()==EncounterResult::Running && battle.Colliders()[0].enabled,
        "player shot cannot damage turret through cover");

    WeaponInventory inventory;
    const auto supplyDirection=WeaponInventory::SupplyPosition()-eye;
    const auto firstId=inventory.EquippedId();
    check(!inventory.Interact(eye,supplyDirection,empty),"armed player cannot duplicate supplies");
    check(inventory.Drop(eye,{0,0,-1}) && !inventory.Armed() && inventory.Drops().size()==1,
        "drop changes equipped weapon into one intact world item");
    check(!inventory.Drop(eye,{0,0,-1}) && inventory.Drops().size()==1,"empty hands cannot duplicate a drop");
    const auto dropped=inventory.Drops()[0].position;
    check(inventory.Hint(eye,dropped-eye,empty)==WeaponInteraction::Pickup,
        "nearby aimed dropped weapon offers pickup");
    check(inventory.Interact(eye,dropped-eye,empty) && inventory.EquippedId()==firstId
        && inventory.Drops().empty() && inventory.Pickups()==1,"pickup consumes world item once and preserves its instance ID");
    check(!inventory.Interact(eye,dropped-eye,empty),"holding E cannot acquire an already collected item");
    check(!inventory.Break(firstId+100) && inventory.Armed(),"old weapon bullet cannot break a different equipped weapon");
    check(inventory.Break(firstId) && !inventory.Armed() && inventory.Breaks()==1
        && !inventory.Break(firstId),"matching weapon breaks exactly once and leaves no recoverable drop");
    check(!inventory.Interact(eye,{1,0,0},empty),"supply requires looking at station");
    check(!inventory.Interact({0,1.65f,10},supplyDirection,empty),"distant supply cannot be used");
    const std::array<Collider,1> nearbyWall{{{100,HitKind::Wall,{-2,0,-0.6f},{2,3,-0.5f}}}};
    check(!inventory.Interact(eye,supplyDirection,nearbyWall),"supply cannot be used through a wall");
    check(inventory.Interact(eye,supplyDirection,empty) && inventory.EquippedId()!=firstId
        && inventory.Supplies()==1,"empty player resupplies one new uniquely identified weapon");
    check(!inventory.Break(firstId) && inventory.Armed(),"resupplied gun survives an old source ID impact");
    inventory.Reset();
    check(inventory.Armed() && inventory.Drops().empty() && inventory.Supplies()==0
        && inventory.Pickups()==0 && inventory.Breaks()==0,"reset restores equipment and clears all weapon state");
    check(!inventory.Drop(eye,{0,0,0}) && inventory.Armed(),"invalid drop input preserves equipped weapon");
    inventory.Drop(eye,{0,0,-1});
    check(!inventory.Interact(eye,{std::numeric_limits<float>::quiet_NaN(),0,0},empty),
        "invalid interaction direction cannot equip a weapon");
    check(!inventory.Interact(eye,inventory.Drops()[0].position-eye,nearbyWall),
        "nearby drop cannot be picked through a wall");

    range.Reset(); range.ToggleCover(); range.Fire(eye,{0,0,-1});
    const auto firedId=range.Flights()[0].projectile.sourceWeaponId;
    range.DropWeapon(eye,{0,0,-1}); range.Interact(eye,supplyDirection);
    for(int i=0;i<100;++i) range.Update(0.01f,0.01f);
    check(range.RemainingTargets()==2 && range.Inventory().Armed()
        && range.Inventory().EquippedId()!=firedId && range.Inventory().Breaks()==0,
        "in-flight old shot destroys target without breaking resupplied gun");
    range.Reset(); range.ToggleCover(); range.Fire(eye,{0,0,-1});
    for(int i=0;i<100;++i) range.Update(0.01f,0.01f);
    check(!range.Inventory().Armed() && !range.Fire(eye,{0,0,-1})
        && range.Inventory().Breaks()==1 && range.Inventory().Drops().size()==1,
        "valid body hit breaks source gun and empty player cannot shoot");
    check(range.Interact(eye,supplyDirection) && range.Fire(eye,{0,1,0}),
        "supply restores ability to shoot after valid-hit breakage");
    for(int i=0;i<500;++i) range.Update(0.01f,0.01f);
    check(range.Inventory().Armed(),"miss does not consume weapon");
    range.Reset(); range.Fire(eye,{0,0,-1});
    for(int i=0;i<100;++i) range.Update(0.01f,0.01f);
    check(range.Inventory().Armed() && range.Inventory().Breaks()==0,"wall hit does not break weapon");

    battle.Reset();
    battle.Fire(eye,glm::vec3(0.85f,1.65f,-9.2f)-eye); tick(100);
    check(battle.Result()==EncounterResult::Running && battle.Colliders()[0].enabled
        && !battle.Colliders()[4].enabled && battle.Phase()==TurretPhase::Disarmed
        && battle.Inventory().Armed(),"enemy weapon hit disarms turret without damaging body or player gun");
    const auto shotsBeforeDisarm=battle.EnemyShots(); tick(500);
    check(battle.EnemyShots()==shotsBeforeDisarm,"disarmed turret stops firing");
    battle.Fire(eye,{0,0,-1}); tick(100);
    check(battle.Result()==EncounterResult::Cleared && battle.Inventory().Drops().empty(),
        "destroying disarmed enemy never drops a broken gun");
    battle.Reset(); battle.Fire(eye,{0,0,-1}); tick(100);
    check(!battle.Inventory().Armed() && battle.Inventory().Breaks()==1
        && battle.Inventory().Drops().size()==1,"intact enemy gun drops once on body destruction");
    tick(200);
    check(battle.Inventory().Drops().size()==1,"cleared encounter cannot duplicate enemy drop");

    battle.Reset();
    const glm::vec3 interceptDirection{-0.3f,0.3f,-1};
    for(int i=0;i<260;++i) battle.Update(0.01f,0.01f,eye,interceptDirection);
    check(!battle.Inventory().Armed() && battle.Inventory().Breaks()==1
        && battle.Health()==100 && battle.HitsTaken()==0 && battle.BlockedEnemyShots()==1,
        "first enemy impact on held gun breaks only gun and stops the same bullet");
    tick(250);
    check(battle.Health()==75 && battle.HitsTaken()==1,"following enemy shot damages empty-handed player");
    battle.Reset(); tick(1200);
    check(!battle.Interact(eye,supplyDirection) && !battle.DropWeapon(eye,{0,0,-1}),
        "failed encounter rejects pickup and drop actions");
    std::cout << checks << " checks, " << failures << " failures\n";
    return failures ? 1 : 0;
}
