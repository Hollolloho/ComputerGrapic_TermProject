#include "Camera.h"
#include "player_view.h"
#include "combat_map_layout.generated.h"
#include "combat_time.h"
#include "combat_feedback.h"
#include "combat.h"
#include "combat_range.h"
#include "turret_encounter.h"
#include "analysis_session.h"
#include "boss_encounter.h"
#include "campaign_flow.h"
#include "terminal_layout.generated.h"
#include "corridor_layout.generated.h"

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

    const auto& corridor=aegis::corridor_asset::walls;
    hit=FindFirstHit(eye,eye+glm::vec3(-5,0,0),corridor);
    check(hit && hit->objectId==10001,"corridor left wall blocks bullet");
    hit=FindFirstHit(eye,eye+glm::vec3(5,0,0),corridor);
    check(hit && hit->objectId==10002,"corridor right wall blocks bullet");
    hit=FindFirstHit(eye,{0,1.65f,-20},corridor);
    check(hit && (hit->objectId==10010 || hit->objectId==10011),"closed door leaves block corridor exit shot");
    hit=FindFirstHit({-0.72f,1.65f,0},{-0.72f,1.65f,-20},corridor);
    check(hit && hit->objectId==10010,"left closed door has transformed world bounds");
    hit=FindFirstHit({0.72f,1.65f,0},{0.72f,1.65f,-20},corridor);
    check(hit && hit->objectId==10011,"right closed door has transformed world bounds");
    hit=FindFirstHit({-1.2f,0.8f,0},{-1.2f,0.8f,-12},corridor);
    check(hit && hit->objectId==10009,"rotated visible cabinet cover blocks low bullet");
    check(!FindFirstHit(eye,{0,1.65f,-9},corridor),"corridor centre line to enemy stays open");
    hit=FindFirstHit(eye,{0,-1,-5},corridor);
    check(hit && hit->objectId==10000,"corridor floor blocks downward bullet");
    hit=FindFirstHit(eye,{0,5,-5},corridor);
    check(hit && hit->objectId==10003,"corridor ceiling blocks upward bullet");
    battle.SetEnvironment(corridor);
    check(battle.Colliders().size()==17 && !battle.Colliders()[1].enabled
        && !battle.Colliders()[2].enabled && !battle.Colliders()[3].enabled,
        "corridor replaces legacy test walls and keeps enemy hitboxes");
    battle.ToggleCover();
    check(!battle.CoverEnabled(),"corridor cannot toggle invisible legacy cover");
    battle.Fire(eye,{0,0,-1}); tick(100);
    check(battle.Result()==EncounterResult::Cleared && !battle.Inventory().Armed(),
        "corridor combat can clear and apply source gun breakage");
    check(battle.Interact(eye,supplyDirection) && battle.Inventory().Armed(),
        "corridor supply is reachable and restores gun after victory");
    battle.Reset(); tick(1200);
    check(battle.Result()==EncounterResult::Failed && battle.Health()==0,
        "corridor enemy shots can cause player failure");
    battle.Reset();
    check(battle.Health()==100 && battle.Colliders().size()==17 && battle.Inventory().Armed()
        && battle.Inventory().Drops().empty(),"corridor restart preserves environment and restores battle state");
    battle.SetEnvironment(empty);
    check(battle.Colliders().size()==5 && battle.Colliders()[2].enabled,
        "return to legacy tests removes corridor collision state");

    DroneBehavior ai;
    check(ai.Phase()==DronePhase::Patrol && ai.Body().enabled && ai.Weapon().enabled,
        "drone starts intact on its fixed patrol route");
    ai.Update(0.5f,eye,empty);
    check(close(ai.Position().x,-0.325f),"drone patrol advances at game-time speed");
    const auto pausedPosition=ai.Position();
    ai.Update(0,eye,empty); ai.Update(-1,eye,empty);
    ai.Update(std::numeric_limits<float>::infinity(),eye,empty);
    check(ai.Position()==pausedPosition && ai.Phase()==DronePhase::Patrol,
        "paused or invalid game time cannot move or fire drone");
    ai.Reset();
    for(int i=0;i<151;++i) ai.Update(0.01f,eye,empty);
    check(ai.Phase()==DronePhase::Warning,"drone patrol transitions into aim warning when player is visible");
    const auto aimingPosition=ai.Position(); const auto lockedAim=ai.AimPoint();
    bool fired=false;
    for(int i=0;i<50;++i) fired=ai.Update(0.01f,eye,empty)||fired;
    check(!fired && ai.Position()==aimingPosition,"warning stops patrol and cannot fire early");
    for(int i=0;i<60;++i) fired=ai.Update(0.01f,eye+glm::vec3(0.4f,0,0),empty)||fired;
    check(fired && ai.Phase()==DronePhase::Waiting && ai.AimPoint()==lockedAim,
        "warning fires once toward locked aim point then enters wait");
    int extraShots=0;
    for(int i=0;i<100;++i) extraShots+=ai.Update(0.01f,eye,empty) ? 1:0;
    check(extraShots==0,"waiting drone cannot fire repeatedly");
    for(int i=0;i<70;++i) ai.Update(0.01f,eye,empty);
    check(ai.Phase()==DronePhase::Warning,"drone reacquires visible player after waiting");
    const std::array<Collider,1> sightWall{{{800,HitKind::Wall,{-4,0,-6},{4,4,-5.8f}}}};
    ai.Update(0.01f,eye,sightWall);
    check(ai.Phase()==DronePhase::Patrol,"occlusion cancels drone warning immediately");
    bool throughWallShot=false; bool moved=false;
    const auto beforeHidden=ai.Position();
    for(int i=0;i<500;++i) { throughWallShot=ai.Update(0.01f,eye,sightWall)||throughWallShot; moved=moved || ai.Position()!=beforeHidden; }
    check(!throughWallShot && moved && ai.Phase()==DronePhase::Patrol,"hidden player causes continued patrol without wall shots");
    const std::array<Collider,2> constrained{{sightWall[0],{801,HitKind::Wall,{1.8f,0,-12},{2,4,-8}}}};
    ai.Reset(); bool safePatrol=true;
    for(int i=0;i<1000;++i) { ai.Update(0.01f,eye,constrained); safePatrol=safePatrol && ai.Body().maximum.x<1.8001f; }
    check(safePatrol,"swept drone body refuses patrol steps into scenery");
    DroneBehavior coarse,fine;
    for(int i=0;i<23;++i) coarse.Update(0.1f,eye,sightWall);
    for(int i=0;i<230;++i) fine.Update(0.01f,eye,sightWall);
    check(close(coarse.Position().x,fine.Position().x),"patrol turnaround preserves speed across frame subdivisions");
    ai.Reset(); ai.Update(1.51f,eye,empty); ai.Disarm();
    check(ai.Phase()==DronePhase::Disarmed && !ai.Weapon().enabled && ai.Body().enabled,
        "drone weapon disarm leaves body intact");
    check(!ai.Update(10,eye,empty),"disarmed drone cannot fire again");
    ai.Destroy();
    check(!ai.Body().enabled && !ai.Weapon().enabled,"destroyed drone disables body and weapon");
    ai.Reset();
    check(ai.Phase()==DronePhase::Patrol && close(ai.Position().x,-0.65f) && ai.Weapon().enabled,
        "drone restart restores patrol, position and weapon");

    battle.SetEnvironment(corridor); battle.SetDroneMode(true);
    tick(150);
    check(battle.IsDroneMode() && battle.EnemyShots()==0 && battle.Health()==100,
        "drone encounter allows patrol before any attack");
    tick(110);
    check(battle.EnemyShots()==1 && battle.Health()==100 && battle.EnemyFlights().size()==1,
        "drone launches a visible projectile after its full warning");
    tick(150);
    check(battle.Health()==75 && battle.HitsTaken()==1,"drone projectile applies existing player damage rules");
    battle.Reset();
    const auto gun=battle.Colliders()[4];
    check(battle.Fire(eye,(gun.minimum+gun.maximum)*0.5f-eye),"player can aim at separate drone gun hitbox");
    // Freeze patrol by letting it acquire a stationary player before weapon test below.
    battle.Reset(); tick(160);
    const auto stationaryGun=battle.Colliders()[4];
    battle.Fire(eye,(stationaryGun.minimum+stationaryGun.maximum)*0.5f-eye); tick(100);
    check(battle.Result()==EncounterResult::Running && !battle.Colliders()[4].enabled
        && battle.DroneState().Phase()==DronePhase::Disarmed && battle.Inventory().Armed(),
        "hit on actual drone weapon stops its firing and preserves player gun");
    const auto disarmedShots=battle.EnemyShots(); tick(500);
    check(battle.EnemyShots()==disarmedShots,"disarmed drone does not generate later shots");
    battle.Fire(eye,battle.DroneState().Position()-eye); tick(100);
    check(battle.Result()==EncounterResult::Cleared && battle.Inventory().Drops().empty(),
        "destroyed drone with broken gun never drops a weapon");
    battle.Reset(); battle.Fire(eye,battle.DroneState().Position()-eye); tick(100);
    check(battle.Result()==EncounterResult::Cleared && !battle.Inventory().Armed()
        && battle.Inventory().Drops().size()==1 && battle.DroneState().Phase()==DronePhase::Destroyed,
        "drone body hit destroys source gun and drops intact enemy gun once");
    battle.Reset(); tick(1600);
    check(battle.Result()==EncounterResult::Failed && battle.Health()==0 && battle.HitsTaken()==4,
        "drone encounter reaches existing four-hit failure");
    battle.Reset();
    check(battle.Health()==100 && battle.EnemyFlights().empty() && battle.PlayerFlights().empty()
        && battle.Inventory().Armed() && battle.Inventory().Drops().empty()
        && battle.DroneState().Phase()==DronePhase::Patrol,
        "drone encounter restart clears projectiles, damage, drops and AI");

    battle.SetDroneMode(false); battle.SetEnvironment(corridor);
    battle.SetExitDoors(10010,10011,aegis::corridor_asset::doorTravel);
    check(!battle.CanAnalyze() && battle.DoorProgress()==0,"live enemy locks analysis and exit door");
    tick(120); // First hostile shot is already travelling when player fires.
    battle.Fire(eye,{0,0,-1}); tick(85);
    check(!battle.EnemyAlive() && battle.Result()==EncounterResult::Securing
        && battle.EnemyFlights().size()==1 && battle.Health()==100,
        "last enemy death preserves its in-flight hostile projectile");
    check(!battle.CanAnalyze() && battle.DoorProgress()==0 && !battle.Fire(eye,{0,0,-1}),
        "pending enemy shots lock analysis, door and additional firing");
    const auto pendingPosition=battle.EnemyFlights()[0].projectile.position;
    battle.Update(0,0,eye);
    check(battle.Result()==EncounterResult::Securing && battle.EnemyFlights()[0].projectile.position==pendingPosition,
        "pause freezes securing state and hostile projectile");
    const auto shotsAtDeath=battle.EnemyShots(); tick(60);
    check(battle.Health()==75 && battle.HitsTaken()==1 && battle.Result()==EncounterResult::Cleared
        && battle.EnemyFlights().empty() && battle.EnemyShots()==shotsAtDeath,
        "dead enemy projectile still deals one hit and sector clears only after it settles");
    check(battle.CanAnalyze(),"safe sector enables analysis permission");
    const float beforeDoor=battle.DoorProgress(); tick(50);
    check(close(battle.DoorProgress(),std::min(1.0f,beforeDoor+0.5f)),"exit leaves open over game time after sector clear");
    const float doorPaused=battle.DoorProgress(); battle.Update(0.5f,0,eye);
    check(battle.DoorProgress()==doorPaused,"pause freezes exit door animation");
    tick(70);
    check(close(battle.DoorProgress(),1),"exit door stops at full open distance");
    hit=FindFirstHit(eye,{0,1.65f,-20},std::span<const Collider>(battle.Colliders()).subspan(5));
    check(!hit,"opening visible exit leaves also clears centre bullet collision");
    battle.Reset();
    hit=FindFirstHit(eye,{0,1.65f,-20},std::span<const Collider>(battle.Colliders()).subspan(5));
    check(battle.DoorProgress()==0 && !battle.CanAnalyze() && battle.EnemyAlive() && hit,
        "restart closes collision and door and resets analysis permission");
    // Enemy dies first, but the already-fired last shot may still kill the player.
    for(int i=0;i<1500 && battle.EnemyShots()<4;++i) battle.Update(0.01f,0.01f,eye);
    check(battle.Health()==25 && battle.EnemyShots()==4,"fatal pending-shot scenario begins at one remaining hit point tier");
    battle.Fire(eye,{0,0,-1}); tick(85);
    check(battle.Result()==EncounterResult::Securing && !battle.EnemyAlive(),
        "last kill can enter securing while a lethal hostile shot remains");
    tick(100);
    check(battle.Result()==EncounterResult::Failed && battle.Health()==0 && !battle.CanAnalyze()
        && battle.DoorProgress()==0,"post-kill lethal hit fails sector instead of opening exit");
    // Remaining shot misses a relocated player and must expire normally.
    battle.Reset(); tick(120); battle.Fire(eye,{0,0,-1}); tick(85);
    for(int i=0;i<500;++i) battle.Update(0.01f,0.01f,{2,1.65f,0});
    check(battle.Result()==EncounterResult::Cleared && battle.Health()==100 && battle.EnemyFlights().empty(),
        "missed hostile projectile expires before safe completion without extra damage");
    battle.Reset(); tick(50);
    check(!battle.CanAnalyze(),"analysis remains locked during live enemy warning");

    AnalysisSession terminal;
    const auto& terminalBox=aegis::terminal_asset::collider;
    const auto terminalDirection=aegis::terminal_asset::interactionPoint-eye;
    const std::array<Collider,1> terminalObstacle{{terminalBox}};
    check(terminal.TargetDistance(terminalBox,eye,terminalDirection,terminalObstacle).has_value(),
        "near aimed terminal can be selected despite its own collision proxy");
    check(!terminal.Open(false,terminalBox,eye,terminalDirection,terminalObstacle)
        && terminal.Entries()==0 && !terminal.Active(),"unsafe sector rejects terminal entry without an event");
    check(!terminal.Open(true,terminalBox,eye,{0,0,-1},terminalObstacle),"terminal requires looking at it");
    check(!terminal.Open(true,terminalBox,{0,1.65f,8},terminalDirection,terminalObstacle),
        "distant terminal rejects interaction");
    check(!terminal.Open(true,terminalBox,eye,{0,0,0},terminalObstacle)
        && !terminal.Open(true,terminalBox,eye,{std::numeric_limits<float>::quiet_NaN(),0,0},terminalObstacle),
        "invalid terminal direction cannot enter analysis");
    check(!terminal.Open(true,terminalBox,eye,terminalDirection,nearbyWall),
        "wall between eye and terminal blocks analysis entry");
    const auto terminalDistance=*terminal.TargetDistance(terminalBox,eye,terminalDirection,terminalObstacle);
    check(!terminal.Open(true,terminalBox,eye,terminalDirection,terminalObstacle,terminalDistance-0.1f),
        "nearer weapon takes priority over terminal for the same interaction");
    check(terminal.Open(true,terminalBox,eye,terminalDirection,terminalObstacle,terminalDistance+0.1f)
        && terminal.Active() && terminal.Entries()==1,"safe nearest terminal emits one entry revision");
    check(!terminal.Open(true,terminalBox,eye,terminalDirection,terminalObstacle)
        && terminal.Entries()==1,"already active analysis cannot emit duplicate entries");
    check(terminal.Close() && !terminal.Active() && terminal.Exits()==1,"closing analysis emits one exit revision");
    check(!terminal.Close() && terminal.Exits()==1,"already closed analysis cannot emit duplicate exits");
    terminal.Open(true,terminalBox,eye,terminalDirection,terminalObstacle); terminal.Reset();
    check(!terminal.Active() && terminal.Entries()==2 && terminal.Exits()==2,
        "reset closes session and preserves monotonic signals for camera and time owners");
    inventory.Reset();
    check(!inventory.InteractionDistance(eye,supplyDirection,empty),"armed inventory offers no pickup distance");
    inventory.Drop(eye,{0,0,-1});
    check(inventory.InteractionDistance(eye,inventory.Drops()[0].position-eye,empty).has_value(),
        "empty inventory exposes aimed pickup distance for one-target arbitration");
    battle.Reset();
    check(!terminal.Open(battle.CanAnalyze(),terminalBox,eye,terminalDirection,terminalObstacle),
        "live encounter cannot enter terminal analysis");
    battle.Fire(eye,{0,0,-1}); tick(100);
    check(terminal.Open(battle.CanAnalyze(),terminalBox,eye,terminalDirection,terminalObstacle),
        "completed encounter enables terminal entry through shared safe flag");
    terminal.Close();
    battle.Reset(); tick(120); battle.Fire(eye,{0,0,-1}); tick(85);
    check(battle.Result()==EncounterResult::Securing
        && !terminal.Open(battle.CanAnalyze(),terminalBox,eye,terminalDirection,terminalObstacle),
        "pending hostile projectile prevents terminal entry after last kill");
    tick(100);
    check(terminal.Open(battle.CanAnalyze(),terminalBox,eye,terminalDirection,terminalObstacle),
        "settled hostile projectile enables terminal analysis");

    BossEncounter boss;
    const auto firingEye=BossEncounter::FiringRoot()+glm::vec3(0,1.65f,0);
    const auto coveredEye=BossEncounter::CoverRoot()+glm::vec3(0,1.65f,0);
    auto closedPose=MakeBossPose(0,firingEye),openPose=MakeBossPose(1,firingEye);
    const auto core=BossCoreCenter(openPose);
    hit=FindBossHit(firingEye,core+(core-firingEye)*0.2f,closedPose,false);
    check(hit && hit->kind==HitKind::Armor,"closed boss panels block actual core ray");
    hit=FindBossHit(firingEye,core+(core-firingEye)*0.2f,openPose,true);
    check(hit && hit->kind==HitKind::Body && hit->objectId==21000,
        "open boss panels expose weakspot through precise part geometry");
    hit=FindBossHit(firingEye,core+(core-firingEye)*0.2f,openPose,false);
    check(hit && hit->kind==HitKind::Armor,"closing or inactive core rejects damage even while panels remain apart");
    const auto bossTick=[&](int frames,const glm::vec3& location) { for(int i=0;i<frames;++i) boss.Update(0.01f,0.01f,location); };
    check(boss.Fire(firingEye,boss.CoreCenter()-firingEye),"boss scene accepts initial player shot");
    bossTick(70,coveredEye);
    check(boss.CoreHits()==0 && boss.BlockedShots()==1 && boss.Inventory().Armed(),
        "closed armor hit preserves player gun and boss health");
    boss.Reset(); bossTick(119,firingEye);
    check(boss.EnemyShots()==0 && boss.Health()==100,"boss warning precedes visible enemy fire");
    bossTick(10,firingEye);
    check(boss.EnemyShots()==2 && boss.Health()==100 && boss.EnemyFlights().size()==2,
        "boss paired cannons emit travelling bullets after warning");
    bossTick(120,firingEye);
    check(boss.Health()==50,"boss paired bullets use one 25-point hit per bullet");
    boss.Reset(); bossTick(900,coveredEye);
    check(boss.Health()==100 && boss.EnemyShots()==0,"physical room cover blocks boss firing at covered player");
    boss.Reset();
    for(int i=0;i<500;++i) boss.Update(0.01f,0.001f,firingEye);
    check(boss.Phase()==BossPhase::Warning && boss.EnemyShots()==0,"boss pattern obeys supplied slow game time");
    boss.Update(1,0,firingEye);
    check(boss.Phase()==BossPhase::Warning && boss.EnemyShots()==0,"pause stops boss phase and projectiles");
    boss.Reset();
    for(int i=0;i<1200 && boss.Phase()!=BossPhase::Exposed;++i) boss.Update(0.01f,0.01f,coveredEye);
    check(boss.Phase()==BossPhase::Exposed && close(boss.ArmorOpen(),1),"only fully open armor enables boss weakspot");
    boss.Fire(firingEye,boss.CoreCenter()-firingEye);
    bossTick(26,coveredEye); boss.Fire(firingEye,boss.CoreCenter()-firingEye);
    bossTick(100,coveredEye);
    check(boss.CoreHits()==1 && boss.Phase()!=BossPhase::Exposed && !boss.Inventory().Armed(),
        "one exposure accepts only one valid core hit and breaks matching player gun");
    check(!boss.Fire(firingEye,boss.CoreCenter()-firingEye),"empty-handed boss player cannot fire");
    check(boss.Interact(coveredEye,aegis::boss_asset::supplyInteraction-coveredEye)
        && boss.Inventory().Armed() && boss.Inventory().Supplies()==1,
        "boss cover-side station restores one weapon after core hit");
    check(!boss.Interact(coveredEye,aegis::boss_asset::supplyInteraction-coveredEye),"boss station cannot duplicate armed weapons");
    for(int hitIndex=1;hitIndex<3;++hitIndex) {
        for(int i=0;i<1200 && boss.Phase()!=BossPhase::Exposed;++i) boss.Update(0.01f,0.01f,coveredEye);
        boss.Fire(firingEye,boss.CoreCenter()-firingEye); bossTick(100,coveredEye);
        if (hitIndex<2) boss.Interact(coveredEye,aegis::boss_asset::supplyInteraction-coveredEye);
    }
    check(boss.CoreHits()==3 && boss.Result()==EncounterResult::Cleared && boss.Health()==100,
        "three separate core exposures and resupply cycles complete boss encounter");
    check(boss.Inventory().Breaks()==3 && boss.Inventory().Supplies()==2 && !boss.Inventory().Armed(),
        "boss victory preserves three breakages and two refills");
    check(!boss.Fire(firingEye,boss.CoreCenter()-firingEye),"boss victory rejects extra shots");
    bossTick(500,firingEye);
    check(boss.CoreHits()==3 && boss.Result()==EncounterResult::Cleared,"boss victory cannot be counted repeatedly");
    boss.Reset(); bossTick(1600,firingEye);
    check(boss.Result()==EncounterResult::Failed && boss.Health()==0 && boss.EnemyFlights().empty(),
        "boss encounter fails after four body impacts and clears flights");
    boss.Reset();
    check(boss.CoreHits()==0 && boss.Health()==100 && boss.Inventory().Armed()
        && boss.Phase()==BossPhase::Warning && boss.ArmorOpen()==0
        && boss.PlayerFlights().empty() && boss.EnemyFlights().empty(),
        "boss restart resets phase, armor, core, health, weapon and all bullets");

    boss.Reset();
    for(int i=0;i<1200 && boss.Phase()!=BossPhase::Exposed;++i) boss.Update(0.01f,0.01f,coveredEye);
    boss.Fire(firingEye,boss.CoreCenter()-firingEye);
    const auto oldBossGun=boss.Inventory().EquippedId();
    boss.DropWeapon(coveredEye,{0,0,1}); boss.Interact(coveredEye,aegis::boss_asset::supplyInteraction-coveredEye);
    bossTick(100,coveredEye);
    check(boss.CoreHits()==1 && boss.Inventory().Armed() && boss.Inventory().EquippedId()!=oldBossGun,
        "old boss weakspot projectile cannot break a newly refilled gun");
    boss.Reset();
    for(int n=0;n<2;++n) {
        for(int i=0;i<1200 && boss.Phase()!=BossPhase::Exposed;++i) boss.Update(0.01f,0.01f,coveredEye);
        boss.Fire(firingEye,boss.CoreCenter()-firingEye); bossTick(100,coveredEye);
        boss.Interact(coveredEye,aegis::boss_asset::supplyInteraction-coveredEye);
    }
    // Final volley aims through the room entrance; it remains in flight after the core hit.
    const glm::vec3 distantEye{0,1.65f,7};
    for(int i=0;i<1200 && boss.Phase()!=BossPhase::Exposed;++i) boss.Update(0.01f,0.01f,distantEye);
    boss.Fire(firingEye,boss.CoreCenter()-firingEye); bossTick(65,coveredEye);
    check(boss.CoreHits()==3 && boss.Result()==EncounterResult::Securing && !boss.EnemyFlights().empty(),
        "third boss hit retains already-fired hostile bullets before victory");
    bossTick(500,coveredEye);
    check(boss.Result()==EncounterResult::Cleared && boss.EnemyFlights().empty(),
        "boss victory waits for remaining volley to settle");

    aegis::CampaignFlow flow;
    using Stage=aegis::CampaignStage;
    using Screen=aegis::CampaignScreen;
    check(flow.Screen()==Screen::Title && !flow.WorldRuns(),"campaign title keeps world stopped");
    check(!flow.Advance(true) && !flow.Retry(),"title cannot skip into battle or retry an absent checkpoint");
    check(flow.ShowHelp() && flow.Screen()==Screen::Help && !flow.WorldRuns(),"Korean help screen does not start battle");
    check(!flow.Start(),"help requires returning to title before start");
    flow.ToTitle();
    check(flow.Start() && flow.Stage()==Stage::Entry && flow.Screen()==Screen::Playing && !flow.WorldRuns(),
        "campaign start enters safe entry room before any battle");
    check(!flow.Start(),"repeated start cannot skip entry room");
    check(flow.Advance(false) && flow.Stage()==Stage::Corridor && flow.WorldRuns(),
        "entry action begins corridor checkpoint");
    check(!flow.Advance(false) && flow.Stage()==Stage::Corridor,"unsafe corridor prevents premature core-room entry");
    check(!flow.ObserveBattle(false,true) && flow.Screen()==Screen::Playing,
        "corridor clear leaves time for analysis and explicit next-room choice");
    check(flow.TogglePause() && !flow.WorldRuns() && flow.Screen()==Screen::Paused,"campaign pause stops world but preserves checkpoint");
    check(!flow.ObserveBattle(true,true) && flow.Stage()==Stage::Corridor,"paused world cannot change battle outcome");
    check(!flow.Advance(true),"paused menu cannot advance rooms");
    check(flow.TogglePause() && flow.WorldRuns(),"resume restores playing state");
    check(flow.ObserveBattle(true,false) && flow.Screen()==Screen::Failed && !flow.WorldRuns(),"corridor failure enters result screen");
    check(flow.Retry() && flow.Stage()==Stage::Corridor && flow.WorldRuns(),"retry restarts corridor checkpoint instead of whole campaign");
    check(flow.Advance(true) && flow.Stage()==Stage::Boss,"safe completed corridor enables boss checkpoint");
    check(!flow.Advance(true),"boss checkpoint cannot be skipped into victory");
    check(flow.ObserveBattle(true,true) && flow.Screen()==Screen::Failed,"simultaneous failure takes priority over victory");
    check(flow.Retry() && flow.Stage()==Stage::Boss,"boss failure retry preserves boss checkpoint");
    check(flow.ObserveBattle(false,true) && flow.Screen()==Screen::Victory && !flow.WorldRuns(),"boss completion enters final victory once");
    check(!flow.ObserveBattle(false,true),"victory cannot be triggered repeatedly");
    check(flow.Restart() && flow.Stage()==Stage::Entry && flow.Screen()==Screen::Playing,"victory restart returns to entry room");
    flow.ToTitle();
    check(flow.Screen()==Screen::Title && flow.Stage()==Stage::Entry && !flow.WorldRuns(),"return to title clears progression checkpoint");
    CombatFeedback feedback;
    const glm::vec3 fxOrigin(0.2f,1.4f,-0.6f),fxRight(1,0,0),fxForward(0,0,-1);
    feedback.Observe(100,0,fxOrigin,fxRight,fxForward);
    check(feedback.Shards().empty() && feedback.DamageStrength()==0,"fresh feedback does not invent damage or broken guns");
    feedback.Observe(75,0,fxOrigin,fxRight,fxForward);
    check(feedback.DamageStrength()==1 && feedback.DamageEvents()==1 && feedback.Shards().empty(),"body damage produces one full flash without weapon fragments");
    feedback.Update(0.14f); feedback.Observe(75,0,fxOrigin,fxRight,fxForward);
    check(std::abs(feedback.DamageStrength()-0.5f)<0.001f && feedback.DamageEvents()==1,"polling unchanged damage cannot restart flash");
    feedback.Observe(75,1,fxOrigin,fxRight,fxForward);
    check(feedback.BreakBursts()==1 && feedback.Shards().size()==18,"committed weapon break produces one bounded fragment burst");
    const auto frozen=feedback.Shards()[0].position;
    const auto strength=feedback.DamageStrength();
    feedback.Update(0); feedback.Observe(75,1,fxOrigin,fxRight,fxForward);
    check(feedback.Shards()[0].position==frozen && feedback.DamageStrength()==strength && feedback.BreakBursts()==1,
        "paused feedback freezes particles and flash without duplicate break events");
    CombatFeedback slow=feedback,fast=feedback;
    slow.Update(0.01f); fast.Update(0.1f);
    check(slow.Shards()[0].age<fast.Shards()[0].age && slow.DamageStrength()>fast.DamageStrength(),"slow game time slows fragment motion and flash decay");
    CombatFeedback whole=feedback,split=feedback;
    whole.Update(0.1f); for(int i=0;i<10;++i) split.Update(0.01f);
    check(glm::length(whole.Shards()[0].position-split.Shards()[0].position)<0.0001f,"ballistic fragments agree across frame subdivisions");
    feedback.Update(-1); feedback.Update(std::numeric_limits<float>::quiet_NaN());
    check(feedback.Shards()[0].position==frozen && feedback.DamageStrength()==strength,"invalid effect time cannot corrupt particles or flash");
    feedback.Update(2);
    check(feedback.Shards().empty() && feedback.DamageStrength()==0,"expired fragments are removed and flash fully fades");
    feedback.Observe(75,1,fxOrigin,fxRight,fxForward);
    check(feedback.Shards().empty() && feedback.BreakBursts()==1,"expired fragments do not respawn from unchanged break counter");
    feedback.Observe(50,2,fxOrigin,fxRight,fxForward);
    check(feedback.DamageEvents()==2 && feedback.BreakBursts()==2 && feedback.Shards().size()==18,"second hit and weapon break create new effects once");
    feedback.Reset();
    check(feedback.Shards().empty() && feedback.DamageStrength()==0 && feedback.BreakBursts()==0 && feedback.DamageEvents()==0,"restart removes particles, flash and observed event counters");
    feedback.Observe(100,0,fxOrigin,fxRight,fxForward);
    check(feedback.Shards().empty() && feedback.DamageStrength()==0,"restart snapshot does not replay old damage or weapon break");
    CombatTime clock;
    auto frameTime=clock.Advance(0.1,0.005);
    check(std::abs(frameTime.gameDt-0.005)<1e-9 && frameTime.realDt==0.1,"combat clock accepts separate real and slow world time");
    check(clock.RequestShot() && !clock.RequestShot(),"one accepted shot locks repeated shot requests");
    frameTime=clock.Advance(0.1,0.005);
    check(std::abs(frameTime.gameDt-0.1)<1e-9 && std::abs(clock.ShotRemaining()-0.15)<1e-9,"stationary shot gives enemies normal time for first part of 0.25 real seconds");
    frameTime=clock.Advance(0.2,0.01);
    check(std::abs(frameTime.gameDt-0.1525)<1e-9 && clock.CanFire(),"frame crossing shot-window boundary splits normal and slow portions exactly");
    frameTime=clock.Advance(0.1,0.005);
    check(std::abs(frameTime.gameDt-0.005)<1e-9,"shot boost returns to supplied world time when expired");
    frameTime=clock.Advance(0.1,0.005,true);
    check(frameTime.gameDt==0.1,"Shift progresses world normally without needing movement");
    frameTime=clock.Advance(0.1,0.005,false);
    check(std::abs(frameTime.gameDt-0.005)<1e-9,"releasing Shift restores external slow time");
    clock.RequestShot(); const double pausedWindow=clock.ShotRemaining();
    frameTime=clock.Advance(1,0.05,true,true);
    check(frameTime.realDt==0 && frameTime.gameDt==0 && clock.ShotRemaining()==pausedWindow,
        "pause overrides Shift and freezes shot window and real-time combat cooldown input");
    frameTime=clock.Advance(0.1,0,true);
    check(frameTime.realDt==0 && frameTime.gameDt==0 && clock.ShotRemaining()==pausedWindow,
        "zero incoming world time freezes combat even during a shot boost");
    clock.Advance(-1,0.05); clock.Advance(0.1,std::numeric_limits<double>::infinity());
    check(clock.ShotRemaining()==pausedWindow,"invalid frame time cannot consume shot window");
    clock.Reset();
    check(clock.CanFire() && clock.ShotRemaining()==0 && clock.Scale()==1,"restart restores time bridge and clears forced shot progress");
    frameTime=clock.Advance(0.1,2);
    check(frameTime.gameDt==0.1,"external time cannot accelerate combat above normal speed");
    for(int fps:{30,60,144}) {
        CombatTime sampled; sampled.RequestShot(); double elapsedGame{};
        for(int i=0;i<fps;++i) elapsedGame+=sampled.Advance(1.0/fps,0.05/fps).gameDt;
        check(std::abs(elapsedGame-0.2875)<1e-8 && sampled.CanFire(),"shot boost integral is consistent across 30/60/144 fps");
    }
    CombatTime projectileClock; CombatRange timedRange;
    timedRange.Fire({0,1.65f,0},{0,0,-1}); projectileClock.RequestShot();
    for(int i=0;i<60;++i) {
        const auto t=projectileClock.Advance(1.0/60,0.05/60);
        timedRange.Update(static_cast<float>(t.realDt),static_cast<float>(t.gameDt));
    }
    check(timedRange.Flights().size()==1 && std::abs(timedRange.Flights()[0].projectile.position.z+3.45f)<0.001f,
        "actual projectile receives 0.25 seconds normal travel then supplied slow time");
    namespace map=aegis::combat_map_asset;
    check(map::playerStart==glm::vec3(0,0,-1.5f) && map::droneSpawn==glm::vec3(0.65f,1.95f,-11.4f),
        "new map starts player and drone at manifest coordinates");
    check(FindFirstHit({-2,1,-5},{-4,1,-5},map::walls).has_value(),"new map left wall blocks bullets");
    check(FindFirstHit({2,1,-5},{4,1,-5},map::walls).has_value(),"new map right wall blocks bullets");
    check(FindFirstHit({0,2,-5},{0,4,-5},map::walls).has_value(),"new map roof blocks bullets");
    check(FindFirstHit({0,1,-5},{0,-1,-5},map::walls).has_value(),"new map floor blocks bullets");
    check(FindFirstHit({-1.9f,1,-5},{-1.9f,1,-10},map::walls).has_value(),"new map A01 cover blocks through its rendered footprint");
    check(FindFirstHit({1.83f,1,-12},{1.83f,1,-17},map::walls).has_value(),"new map A02 cover blocks through its rendered footprint");
    check(!FindFirstHit({0,1,-1},{0,1,1},map::walls),"new map entry doorway is not covered by one oversized wall box");
    check(!FindFirstHit({0,1,-17},{0,1,-19},map::walls),"new map interlock doorway remains open between jambs");
    const auto mapEye=map::playerStart+glm::vec3(0,1.65f,0);
    check(!FindFirstHit(mapEye,map::droneSpawn,map::walls),"new map starting player has an unobstructed drone firing line");
    DroneBehavior placedDrone;
    check(placedDrone.SetSpawn(map::droneSpawn,glm::radians(map::droneYawDegrees),map::droneSpawn.x),"drone accepts manifest position yaw and patrol centre");
    check(placedDrone.Position()==map::droneSpawn && std::abs(placedDrone.Yaw()-glm::radians(map::droneYawDegrees))<0.0001f,
        "drone renders exact initial placement before patrol update");
    for(int i=0;i<200;++i) placedDrone.Update(0.02f,{0,1,50},map::walls);
    check(placedDrone.Position().x>=map::droneSpawn.x-0.65f && placedDrone.Position().x<=map::droneSpawn.x+0.65f
        && placedDrone.Position().y==map::droneSpawn.y && placedDrone.Position().z==map::droneSpawn.z,
        "new map patrol retains manifest height depth and translated route");
    placedDrone.Reset();
    check(placedDrone.Position()==map::droneSpawn,"drone reset restores configured map spawn");
    check(!placedDrone.SetSpawn({99,1,0},0,0) && placedDrone.Position()==map::droneSpawn,
        "invalid translated patrol spawn is rejected without losing placement");
    TurretEncounter mapped;
    mapped.SetEnvironment(map::walls); mapped.SetDroneMode(true);
    mapped.SetDroneSpawn(map::droneSpawn,glm::radians(map::droneYawDegrees),map::droneSpawn.x);
    mapped.SetSupply(map::supply); mapped.Reset();
    mapped.Fire(mapEye,glm::vec3(-1.9f,1,-7.45f)-mapEye);
    for(int i=0;i<65;++i) mapped.Update(0.01f,0.01f,mapEye);
    check(mapped.EnemyAlive() && mapped.Inventory().Armed() && mapped.PlayerFlights().empty(),
        "actual new-map player projectile stops at cover without killing drone or breaking gun");
    mapped.Reset(); mapped.Fire(mapEye,map::droneSpawn-mapEye);
    for(int i=0;i<120;++i) mapped.Update(0.01f,0.01f,mapEye);
    check(mapped.Result()==EncounterResult::Cleared && mapped.Inventory().Breaks()==1 && mapped.Inventory().Drops().size()==1,
        "new-map drone kill clears safe encounter and preserves one valid break and gun drop");
    check(mapped.Interact(mapEye,map::supply-mapEye) && mapped.Inventory().Armed(),
        "new-map supply can rearm from manifest player start");
    mapped.Reset(); mapped.DropWeapon(mapEye,{0,0,-1});
    check(mapped.DroneState().Position()==map::droneSpawn && mapped.Interact(mapEye,map::supply-mapEye),
        "encounter restart preserves new-map spawn and supply configuration");
    aegis::PlayerViewInput playerInput;
    const aegis::PlayerView fallbackView{{4,2,-3},{1,0,0},{0,1,0},55};
    check(!playerInput.Active() && playerInput.Selected(fallbackView).eye==fallbackView.eye,
        "absent external player view preserves legacy camera fallback");
    const glm::vec3 movedEye(1,1.65f,-2.5f);
    check(playerInput.Set({movedEye,map::droneSpawn-movedEye,{0,2,0.1f},45}),"external player accepts world eye direction up and FOV atomically");
    auto selected=playerInput.Selected(fallbackView);
    check(selected.eye==movedEye && std::abs(glm::length(selected.forward)-1)<0.0001f
        && std::abs(glm::length(selected.up)-1)<0.0001f && std::abs(glm::dot(selected.forward,selected.up))<0.0001f,
        "external direction is normalized and camera basis is orthonormal");
    const auto eyeInView=selected.ViewMatrix()*glm::vec4(selected.eye,1);
    const auto aheadInView=selected.ViewMatrix()*glm::vec4(selected.eye+selected.forward,1);
    check(glm::length(glm::vec3(eyeInView))<0.0001f && std::abs(aheadInView.z+1)<0.0001f,
        "render view puts supplied eye at origin and aiming direction down negative Z");
    check(!playerInput.Set({{99,99,99},{0,0,0},{0,1,0},60}) && playerInput.Selected(fallbackView).eye==movedEye,
        "invalid zero direction cannot partially overwrite a valid player snapshot");
    check(!playerInput.Set({{0,0,0},{0,1,0},{0,2,0},60}),"parallel camera up and forward are rejected");
    check(!playerInput.Set({{std::numeric_limits<float>::quiet_NaN(),0,0},{0,0,-1},{0,1,0},60}),
        "non-finite player eye is rejected");
    check(!playerInput.Set({movedEye,{0,0,-1},{0,1,0},34}) && !playerInput.Set({movedEye,{0,0,-1},{0,1,0},71}),
        "external FOV must stay within required 35 to 70 degree range");
    check(playerInput.Set({movedEye,{0,0,-1},{0,1,0},35}) && playerInput.Set({movedEye,{0,0,-1},{0,1,0},70}),
        "both valid FOV endpoints are accepted without moving the eye");
    CombatRange movedRange;
    playerInput.Set({movedEye,map::droneSpawn-movedEye,{0,1,0},45}); selected=playerInput.Selected(fallbackView);
    movedRange.Fire(selected.eye,selected.forward);
    check(movedRange.Flights()[0].projectile.position==movedEye
        && glm::length(glm::normalize(movedRange.Flights()[0].projectile.velocity)-selected.forward)<0.0001f,
        "actual shot starts at externally supplied eye and follows supplied aim");
    WeaponInventory movedInventory; movedInventory.SetSupplyPosition(map::supply);
    movedInventory.Break(movedInventory.EquippedId());
    const glm::vec3 farEye(0,1.65f,2),nearEye(-1.2f,1.65f,-2.4f);
    playerInput.Set({farEye,map::supply-farEye,{0,1,0},60}); selected=playerInput.Selected(fallbackView);
    check(movedInventory.Hint(selected.eye,selected.forward,map::walls)==WeaponInteraction::None,
        "supply does not use stale near-camera position after external player moves away");
    playerInput.Set({nearEye,map::supply-nearEye,{0,1,0},60}); selected=playerInput.Selected(fallbackView);
    check(movedInventory.Hint(selected.eye,selected.forward,map::walls)==WeaponInteraction::Supply
        && movedInventory.Interact(selected.eye,selected.forward,map::walls),
        "moving external player near supply enables aimed rearm");
    movedInventory.Break(movedInventory.EquippedId());
    playerInput.Set({nearEye,nearEye-map::supply,{0,1,0},60}); selected=playerInput.Selected(fallbackView);
    check(movedInventory.Hint(selected.eye,selected.forward,map::walls)==WeaponInteraction::None,
        "external aim away from nearby supply disables interaction");
    playerInput.Clear();
    check(!playerInput.Active() && playerInput.Selected(fallbackView).eye==fallbackView.eye
        && playerInput.Selected(fallbackView).verticalFovDegrees==55,
        "clearing external player restores existing camera pose and FOV without stale state");
    Camera movingCamera; movingCamera.Init();
    check(movingCamera.camPos==glm::vec3(0,1.65f,-1.5f),"friend camera initializes at new-map player eye height");
    const auto straight=movingCamera.Movement(1,true,false,false,false);
    const auto diagonal=movingCamera.Movement(1,true,false,false,true);
    check(glm::length(straight-glm::vec3(0,0,-3))<0.0001f,"W moves horizontally at three metres per real second");
    check(std::abs(glm::length(straight)-glm::length(diagonal))<0.0001f,"W plus D does not increase movement speed");
    check(movingCamera.Movement(1,true,true,true,true)==glm::vec3(0),"opposite movement inputs cancel");
    movingCamera.Rotate(70,90);
    const auto rotatedMove=movingCamera.Movement(0.5f,true,false,false,false);
    check(std::abs(rotatedMove.x+1.5f)<0.0001f && rotatedMove.y==0 && std::abs(rotatedMove.z)<0.0001f,
        "movement follows camera yaw but never camera pitch");
    const auto directionBefore=movingCamera.camTarget-movingCamera.camPos;
    movingCamera.camPos+=rotatedMove; movingCamera.camTarget+=rotatedMove; movingCamera.UpdateView();
    check(glm::length((movingCamera.camTarget-movingCamera.camPos)-directionBefore)<0.0001f,
        "translating camera position and target preserves aim");
    movingCamera.Rotate(1000,0);
    check(movingCamera.rotX==89,"friend camera clamps upward pitch before singularity");
    check(movingCamera.Movement(1,true,false,false,false).y==0,"looking nearly straight up still moves on ground plane");
    movingCamera.Rotate(-2000,0);
    check(movingCamera.rotX==-89,"friend camera clamps downward pitch before singularity");
    check(movingCamera.Movement(0,true,false,false,false)==glm::vec3(0)
        && movingCamera.Movement(-1,true,false,false,false)==glm::vec3(0)
        && movingCamera.Movement(std::numeric_limits<float>::infinity(),true,false,false,false)==glm::vec3(0),
        "stopped or invalid frame time cannot move player");
    Camera sampledCamera; sampledCamera.Init(); glm::vec3 sum(0);
    for(int i=0;i<144;++i) sum+=sampledCamera.Movement(1.0f/144,true,false,false,true);
    check(glm::length(sum-diagonal)<0.0001f,"camera movement agrees across frame subdivisions");
    sampledCamera.UpdateProjection(1600,900); const auto validProjection=sampledCamera.projection;
    sampledCamera.UpdateProjection(0,900); sampledCamera.UpdateProjection(1600,0);
    check(sampledCamera.projection==validProjection,"zero framebuffer size preserves valid camera projection");
    std::cout << checks << " checks, " << failures << " failures\n";
    return failures ? 1 : 0;
}
