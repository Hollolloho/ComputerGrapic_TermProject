#pragma once
#include <glm/glm.hpp>
#include "main/GameMath.h"
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>

namespace aegis
{
// World-space eye, not the floor/root position. FOV affects projection only.
struct PlayerView
{
    glm::vec3 eye{0, 1.65f, 0}, forward{0, 0, -1}, up{0, 1, 0};
    float verticalFovDegrees{60};

    glm::mat4 ViewMatrix() const
    {
        return glm::lookAt(glm::vec3(0), forward, up) * glm::translate(glm::mat4(1), -eye);
    }
};

class PlayerViewInput
{
public:
    bool Set(const PlayerView& input)
    {
        if (!IsFinite(input.eye) || !IsFinite(input.forward) || !IsFinite(input.up) ||
            !std::isfinite(input.verticalFovDegrees) || input.verticalFovDegrees < 35 ||
            input.verticalFovDegrees > 70)
        {
            return false;
        }
        const float f = glm::length(input.forward), u = glm::length(input.up);
        if (!std::isfinite(f) || !std::isfinite(u) || f < 1e-5f || u < 1e-5f)
        {
            return false;
        }
        PlayerView candidate = input;
        candidate.forward /= f;
        const auto right = glm::cross(candidate.forward, input.up / u);
        const float length = glm::length(right);
        if (length < 1e-4f)
        {
            return false;
        }
        candidate.up = glm::normalize(glm::cross(right / length, candidate.forward));
        view = candidate;
        active = true;
        return true;
    }

    void Clear()
    {
        active = false;
    }

    bool Active() const
    {
        return active;
    }

    PlayerView Selected(const PlayerView& fallback) const
    {
        return active ? view : fallback;
    }

private:
    PlayerView view;
    bool active = false;
};
} // namespace aegis
