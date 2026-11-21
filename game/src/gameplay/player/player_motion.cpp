// 플레이어를 벽에 닿기 직전까지만 이동시키고, 막히지 않은 방향으로 미끄러지게 한다.
#include "player_motion.h"
#include "main/GameMath.h"
#include <glm/glm.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace aegis::combat
{
namespace
{
constexpr float Radius = 0.3f, Skin = 0.001f;

bool Blocks(const Collider& wall, float eyeY)
{
    return wall.enabled && (wall.kind == HitKind::Wall || wall.kind == HitKind::Armor) &&
           IsFinite(wall.minimum) && IsFinite(wall.maximum) &&
           glm::all(glm::lessThanEqual(wall.minimum, wall.maximum)) &&
           wall.maximum.y > eyeY - 1.65f + 0.18f && wall.minimum.y < eyeY + 0.15f - Skin;
}

bool Sweep(const glm::vec2& p, const glm::vec2& d, const glm::vec2& low, const glm::vec2& high, float& time,
           glm::vec2& normal)
{
    float enter = -std::numeric_limits<float>::infinity(), exit = std::numeric_limits<float>::infinity();
    normal = glm::vec2(0);
    for (int axis = 0; axis < 2; ++axis)
    {
        if (std::abs(d[axis]) < 1e-8f)
        {
            if (p[axis] < low[axis] - 1e-6f || p[axis] > high[axis] + 1e-6f)
            {
                return false;
            }
            continue;
        }
        float near = (low[axis] - p[axis]) / d[axis], far = (high[axis] - p[axis]) / d[axis];
        const float sign = d[axis] > 0 ? -1.0f : 1.0f;
        if (near > far)
        {
            std::swap(near, far);
        }
        if (near > enter + 1e-6f)
        {
            enter = near;
            normal = glm::vec2(0);
            normal[axis] = sign;
        }
        else if (std::abs(near - enter) <= 1e-6f)
        {
            normal[axis] = sign;
        }
        exit = std::min(exit, far);
        if (enter > exit + 1e-6f)
        {
            return false;
        }
    }
    if (enter < -1e-6f || enter > 1 || exit < 0 || glm::dot(normal, d) >= 0)
    {
        return false;
    }
    time = std::max(0.0f, enter);
    return true;
}
} // namespace

glm::vec3 RequestedPlayerMovement(const glm::vec3& viewDirection, float forwardInput, float rightInput,
                                 float distance)
{
    if (!IsFinite(viewDirection) || !std::isfinite(forwardInput) || !std::isfinite(rightInput) ||
        !std::isfinite(distance) || distance <= 0)
    {
        return glm::vec3(0.f);
    }
    glm::vec3 forward = viewDirection;
    forward.y = 0.f;
    if (glm::length(forward) < 0.001f)
    {
        return glm::vec3(0.f);
    }
    // normalize는 길이를 1로, cross는 시선의 오른쪽 방향으로 만든다.
    forward = glm::normalize(forward);
    const glm::vec3 right = glm::cross(forward, glm::vec3(0.f, 1.f, 0.f));
    const glm::vec3 direction = forward * forwardInput + right * rightInput;
    if (glm::length(direction) < 0.001f)
    {
        return glm::vec3(0.f);
    }
    return glm::normalize(direction) * distance;
}

PlayerMove MovePlayer(const glm::vec3& eye, const glm::vec3& desired, std::span<const Collider> walls)
{
    PlayerMove result{eye, glm::vec3(0), false};
    if (!IsFinite(eye) || !IsFinite(desired))
    {
        return result;
    }
    glm::vec2 p(eye.x, eye.z), remaining(desired.x, desired.z);
    // Recover a small initial overlap before sweeping. Never iterate without a bound.
    for (int pass = 0; pass < 8; ++pass)
    {
        bool pushed = false;
        for (const auto& wall : walls)
        {
            if (!Blocks(wall, eye.y))
            {
                continue;
            }
            const auto low = glm::vec2(wall.minimum.x, wall.minimum.z) - glm::vec2(Radius + Skin);
            const auto high = glm::vec2(wall.maximum.x, wall.maximum.z) + glm::vec2(Radius + Skin);
            if (p.x <= low.x + 1e-6f || p.x >= high.x - 1e-6f || p.y <= low.y + 1e-6f ||
                p.y >= high.y - 1e-6f)
            {
                continue;
            }
            const std::array<float, 4> distance{p.x - low.x, high.x - p.x, p.y - low.y, high.y - p.y};
            const auto side =
                static_cast<int>(std::min_element(distance.begin(), distance.end()) - distance.begin());
            if (side == 0)
            {
                p.x = low.x - Skin;
            }
            else if (side == 1)
            {
                p.x = high.x + Skin;
            }
            else if (side == 2)
            {
                p.y = low.y - Skin;
            }
            else
            {
                p.y = high.y + Skin;
            }
            pushed = true;
            result.blocked = true;
        }
        if (!pushed)
        {
            break;
        }
    }
    for (int pass = 0; pass < 4 && glm::dot(remaining, remaining) > 1e-12f; ++pass)
    {
        float nearest = 2;
        glm::vec2 normal(0);
        for (const auto& wall : walls)
        {
            if (!Blocks(wall, eye.y))
            {
                continue;
            }
            const auto low = glm::vec2(wall.minimum.x, wall.minimum.z) - glm::vec2(Radius + Skin);
            const auto high = glm::vec2(wall.maximum.x, wall.maximum.z) + glm::vec2(Radius + Skin);
            float time{};
            glm::vec2 n;
            if (!Sweep(p, remaining, low, high, time, n))
            {
                continue;
            }
            if (time < nearest - 1e-6f)
            {
                nearest = time;
                normal = n;
            }
            else if (std::abs(time - nearest) <= 1e-6f)
            {
                for (int axis = 0; axis < 2; ++axis)
                {
                    if (n[axis] != 0)
                    {
                        normal[axis] = n[axis];
                    }
                }
            }
        }
        if (nearest > 1)
        {
            p += remaining;
            remaining = glm::vec2(0);
            break;
        }
        p += remaining * nearest;
        remaining *= 1 - nearest;
        result.blocked = true;
        for (int axis = 0; axis < 2; ++axis)
        {
            if (remaining[axis] * normal[axis] < 0)
            {
                remaining[axis] = 0;
            }
        }
    }
    result.eye = {p.x, eye.y, p.y};
    result.displacement = result.eye - eye;
    return result;
}

double MovementTimeScale(const glm::vec3& actual, double realDeltaTime, double normalSpeed)
{
    if (!IsFinite(actual) || !std::isfinite(realDeltaTime) || realDeltaTime <= 0 ||
        !std::isfinite(normalSpeed) || normalSpeed <= 0)
    {
        return 0.05;
    }
    const double x = actual.x, z = actual.z;
    return std::clamp(std::sqrt(x * x + z * z) / (normalSpeed * realDeltaTime), 0.05, 1.0);
}
} // namespace aegis::combat
