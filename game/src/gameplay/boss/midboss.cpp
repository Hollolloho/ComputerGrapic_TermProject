// E형의 코어와 레이저가 같은 장착 좌표와 게임 시간을 사용한다.
#include "midboss.h"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>

namespace aegis::combat
{
namespace
{
const double chargeStart = 1.0, fireStart = 4.0, coolingStart = 6.0, cycleEnd = 10.0;
const double damageInterval = .5, timeEpsilon = .000001;
const float beamLength = 24.f, beamHalfWidth = .38f;
const ObjectId armorId = 54000;

bool ClipAxis(const MidbossBeam& ray, const glm::vec3& offset, const glm::vec3& halfSize,
              const glm::vec3& axis, float& first, float& last)
{
    if (glm::dot(axis, axis) < 1e-8f)
        return true;
    const float center = glm::dot(offset, axis);
    const float speed = glm::dot(ray.direction, axis);
    // 단면의 양 축과 장애물의 세 축을 같은 축에 투영한다.
    const float radius = glm::dot(halfSize, glm::abs(axis)) + ray.halfWidth *
                         (std::abs(glm::dot(ray.right, axis)) + std::abs(glm::dot(ray.up, axis)));
    if (std::abs(speed) < 1e-6f)
        return std::abs(center) <= radius;
    const float a = (-radius - center) / speed, b = (radius - center) / speed;
    first = std::max(first, std::min(a, b));
    last = std::min(last, std::max(a, b));
    return first <= last;
}

bool BeamHitsBox(const MidbossBeam& ray, const Collider& box, float& first)
{
    const glm::vec3 halfSize = (box.maximum - box.minimum) * .5f;
    const glm::vec3 offset = ray.origin - (box.minimum + halfSize);
    const glm::vec3 worldAxes[] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
    const glm::vec3 beamAxes[] = {ray.right, ray.up, ray.direction};
    first = 0;
    float last = ray.length;
    for (int i = 0; i < 3; ++i)
    {
        if (!ClipAxis(ray, offset, halfSize, worldAxes[i], first, last) ||
            !ClipAxis(ray, offset, halfSize, beamAxes[i], first, last))
            return false;
    }
    for (const auto& world : worldAxes)
        for (const auto& axis : beamAxes)
            if (!ClipAxis(ray, offset, halfSize, glm::cross(world, axis), first, last))
                return false;
    return true;
}
} // namespace

void Midboss::SetMount(const glm::vec3& position, float scale)
{
    enabled = true;
    root = glm::scale(glm::translate(glm::mat4(1), position), glm::vec3(scale));
    Reset();
}

void Midboss::Clear()
{
    enabled = false;
    beam = {};
}

void Midboss::Reset()
{
    cycleTime = 0;
    coreHits = 0;
    cooldownHit = false;
    beam = {};
    if (!enabled)
        return;
    UpdatePose();
    beam.origin = glm::vec3(pose.nodes[boss_asset::core_visual] * glm::vec4(0, .01f, .185f, 1));
    beam.halfWidth = beamHalfWidth;
    AimAt(beam.origin + glm::vec3(0, -1.3f, 6.f));
}

LaserPhase Midboss::Phase() const
{
    if (!Alive()) return LaserPhase::Defeated;
    if (cycleTime + timeEpsilon < chargeStart) return LaserPhase::Waiting;
    if (cycleTime + timeEpsilon < fireStart) return LaserPhase::Charging;
    if (cycleTime + timeEpsilon < coolingStart) return LaserPhase::Firing;
    return LaserPhase::Cooling;
}

float Midboss::ChargeProgress() const
{
    return static_cast<float>(std::clamp((cycleTime - chargeStart) / (fireStart - chargeStart), 0.0, 1.0));
}

void Midboss::UpdatePose()
{
    const auto phase = Phase();
    float armorOpen = CoreExposed() ? 1.f : 0.f;
    if (phase == LaserPhase::Charging || phase == LaserPhase::Firing)
        armorOpen = .55f;
    pose = MakeBossPose(root, armorOpen, glm::vec3(root * glm::vec4(0, -2, 8, 1)));
}

void Midboss::AimAt(const glm::vec3& eye)
{
    const glm::vec3 toward = eye - glm::vec3(0, .6f, 0) - beam.origin;
    if (toward.z <= .05f)
        return; // 보스 뒤에 들어가도 레이저가 몸체를 관통해 뒤로 돌지 않는다.
    const float limit = toward.z * 1.4f;
    const glm::vec3 aim(std::clamp(toward.x, -limit, limit), std::max(toward.y, -2.f * toward.z), toward.z);
    beam.direction = glm::normalize(aim);
    beam.right = glm::normalize(glm::cross(glm::vec3(0, 1, 0), beam.direction));
    beam.up = glm::cross(beam.direction, beam.right);
}

void Midboss::ClipBeam(const std::vector<Collider>& walls)
{
    beam.length = beamLength;
    for (const auto& wall : walls)
    {
        if (!wall.enabled || (wall.kind != HitKind::Wall && wall.kind != HitKind::Armor))
            continue;
        float first;
        if (BeamHitsBox(beam, wall, first))
            beam.length = first;
    }
}

int Midboss::Update(float gameDeltaTime, const glm::vec3& eye, const std::vector<Collider>& walls)
{
    if (!Alive() || !std::isfinite(gameDeltaTime) || gameDeltaTime <= 0)
        return 0;
    const auto previousPhase = Phase();
    const bool previousExposure = CoreExposed();
    int damage = 0;
    double remaining = gameDeltaTime;
    while (remaining > 0)
    {
        if (cycleTime + timeEpsilon < fireStart)
            AimAt(eye);
        const double end = std::min(cycleEnd, cycleTime + remaining);
        if (end + timeEpsilon >= chargeStart && cycleTime + timeEpsilon < coolingStart)
            ClipBeam(walls);
        for (double tick = fireStart; tick < coolingStart; tick += damageInterval)
        {
            if (tick <= cycleTime + timeEpsilon || tick > end + timeEpsilon)
                continue;
            float first;
            if (BeamHitsBox(beam, PlayerBodyCollider(0, eye), first) && first < beam.length)
                damage += DamagePerTick;
        }
        remaining -= end - cycleTime;
        cycleTime = end;
        if (cycleTime + timeEpsilon >= cycleEnd)
        {
            cycleTime = 0;
            cooldownHit = false;
        }
        else
            break;
    }
    if (Phase() != previousPhase || CoreExposed() != previousExposure)
        UpdatePose();
    if (Phase() == LaserPhase::Waiting || Phase() == LaserPhase::Cooling)
        beam.length = 0;
    return damage;
}

bool Midboss::FindHit(const glm::vec3& start, const glm::vec3& end, Hit& hit) const
{
    if (!Alive() || !FindBossHit(start, end, pose, CoreExposed(), hit))
        return false;
    hit.objectId = hit.kind == HitKind::Body ? CoreId : armorId;
    return true;
}

void Midboss::HitCore()
{
    if (!CoreExposed())
        return;
    ++coreHits;
    cooldownHit = true;
    UpdatePose();
}
} // namespace aegis::combat
