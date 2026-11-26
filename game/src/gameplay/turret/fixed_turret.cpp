// 제자리 포탑의 조준 경고와 사격 간격을 세계 시간으로 갱신한다.
#include "fixed_turret.h"
#include "main/GameMath.h"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>

namespace aegis::combat
{
namespace
{
const float pivotHeight = 1.62f;
const float barrelReach = 1.115f;
const float twinBarrelOffset = 0.57f;
const float TwinShotInterval = 0.15f;

Collider TurnedBox(ObjectId id, HitKind kind, const glm::mat4& transform,
                   const glm::vec3& low, const glm::vec3& high, bool enabled)
{
    const glm::vec3 halfSize = (high - low) * .5f;
    const glm::vec3 right(transform[0]), up(transform[1]), forward(transform[2]);
    const auto center = glm::vec3(transform * glm::vec4((low + high) * .5f, 1));
    const auto extent = glm::abs(right) * halfSize.x + glm::abs(up) * halfSize.y +
                        glm::abs(forward) * halfSize.z;
    return {id, kind, center - extent, center + extent, enabled, halfSize, right, up};
}
} // namespace

bool FixedTurret::SetPosition(const glm::vec3& point)
{
    if (!IsFinite(point))
    {
        return false;
    }
    position = point;
    Reset();
    return true;
}

void FixedTurret::Configure(TurretType model, const glm::vec3& point, ObjectId index, float rotYDegrees)
{
    type = model;
    number = index;
    spawnRotY = glm::radians(rotYDegrees);
    SetPosition(point);
}

void FixedTurret::Reset()
{
    phase = TurretPhase::Idle;
    timer = 0;
    direction = RotateY({0, 0, 1}, spawnRotY);
    firingBarrel = 0;
    destroyedArmed = false;
    aimPoint = position + glm::vec3(0, pivotHeight, 0) + direction * 5.0f;
}

void FixedTurret::Disarm()
{
    if (Alive())
    {
        phase = TurretPhase::Disarmed;
        timer = 0;
    }
}

void FixedTurret::Destroy()
{
    if (!Alive())
        return;
    destroyedArmed = Armed();
    phase = TurretPhase::Destroyed;
    timer = 0;
}

float FixedTurret::BarrelOffset(int barrel) const
{
    if (type == TurretType::SingleC) return 0;
    return barrel == 0 ? -twinBarrelOffset : twinBarrelOffset;
}

glm::vec3 FixedTurret::Muzzle() const
{
    return Muzzle(firingBarrel);
}

glm::vec3 FixedTurret::Muzzle(int barrel) const
{
    return glm::vec3(BarrelTransform() * glm::vec4(BarrelOffset(barrel), 0, barrelReach, 1));
}

glm::mat4 FixedTurret::HeadTransform() const
{
    const float rotY = std::atan2(direction.x, direction.z);
    return glm::rotate(glm::translate(glm::mat4(1), position), rotY, glm::vec3(0, 1, 0));
}

glm::mat4 FixedTurret::BarrelTransform() const
{
    const float rotX = -std::asin(std::clamp(direction.y, -1.0f, 1.0f));
    const auto pivot = glm::translate(HeadTransform(), {0, pivotHeight, 0});
    return glm::rotate(pivot, rotX, glm::vec3(1, 0, 0));
}

Collider FixedTurret::Body() const
{
    const float halfWidth = type == TurretType::Twin ? 0.925f : 0.81f;
    const float height = type == TurretType::Twin ? 2.118f : 2.18f;
    return {BodyTarget(), HitKind::Body, position + glm::vec3(-halfWidth, 0, -halfWidth),
            position + glm::vec3(halfWidth, height, halfWidth), Alive()};
}

Collider FixedTurret::Weapon(int barrel) const
{
    // 두 OBJ에서 돌출된 포신은 피벗 기준 Z 0.605~1.115, 반 폭과 높이 약 0.20이다.
    const float x = BarrelOffset(barrel);
    return TurnedBox(WeaponTarget(), HitKind::Weapon, BarrelTransform(),
                     {x - .202f, -.202f, .605f}, {x + .202f, .202f, barrelReach},
                     Armed() && (barrel == 0 || type == TurretType::Twin));
}

std::array<Collider, FixedTurret::TargetCount> FixedTurret::Targets() const
{
    // 받침은 제자리에 있고 머리는 Y 회전, Single_C의 노출 몸체는 포신과 함께 X 회전한다.
    auto base = Body();
    base.minimum.z = position.z - .82f;
    base.maximum.z = position.z + .82f;
    base.maximum.y = position.y + .86f;
    const bool twin = type == TurretType::Twin;
    const auto head = TurnedBox(BodyTarget(), HitKind::Body, HeadTransform(),
                               twin ? glm::vec3(-1.025f, .82f, -.725f) : glm::vec3(-.77f, .82f, -.69f),
                               twin ? glm::vec3(1.025f, 2.118f, .78f) : glm::vec3(.77f, 2.18f, .605f),
                               Alive());
    const auto core = TurnedBox(BodyTarget(), HitKind::Body, BarrelTransform(),
                               {-.395f, -.38f, -.53f}, {.395f, .38f, .45f}, Alive() && !twin);
    return {base, head, core, Weapon(), Weapon(1)};
}

bool FixedTurret::Update(float gameDeltaTime, const glm::vec3& eye, const std::vector<Collider>& walls)
{
    if (!Armed() || !std::isfinite(gameDeltaTime) || gameDeltaTime <= 0 || !IsFinite(eye))
    {
        return false;
    }
    if (phase == TurretPhase::Cooldown)
    {
        timer -= gameDeltaTime;
        if (timer <= 0)
        {
            phase = TurretPhase::Idle;
        }
        return false;
    }
    if (phase == TurretPhase::Firing)
    {
        timer -= gameDeltaTime;
        if (timer > 0)
            return false;

        // 첫 탄의 조준점을 유지한 채 오른쪽 포신에서 이어 쏜다.
        firingBarrel = 1;
        phase = TurretPhase::Cooldown;
        timer = 1.8f;
        return !IsBlocked(Muzzle(), aimPoint, walls);
    }
    const bool visible = glm::length(eye - position) < 18 &&
        (!IsBlocked(Muzzle(0), eye, walls) ||
         (type == TurretType::Twin && !IsBlocked(Muzzle(1), eye, walls)));
    if (!visible)
    {
        phase = TurretPhase::Idle;
        timer = 0;
        return false;
    }
    if (phase == TurretPhase::Idle)
    {
        const auto delta = eye - (position + glm::vec3(0, pivotHeight, 0));
        if (glm::dot(delta, delta) < 1e-8f)
        {
            return false;
        }
        direction = glm::normalize(delta);
        aimPoint = eye;
        firingBarrel = 0;
        phase = TurretPhase::Warning;
        timer = 1.2f;
        return false;
    }
    timer -= gameDeltaTime;
    if (timer > 0)
    {
        return false;
    }
    if (type == TurretType::Twin)
    {
        phase = TurretPhase::Firing;
        timer = TwinShotInterval;
    }
    else
    {
        phase = TurretPhase::Cooldown;
        timer = 1.8f;
    }
    return !IsBlocked(Muzzle(), aimPoint, walls);
}
} // namespace aegis::combat
