#include "combat.h"

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
    std::cout << checks << " checks, " << failures << " failures\n";
    return failures ? 1 : 0;
}
