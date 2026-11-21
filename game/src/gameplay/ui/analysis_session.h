#pragma once
#include "gameplay/combat/combat.h"

namespace aegis::combat
{
// Terminal UI gate. Revisions let camera/time owners observe enter/exit once.
class AnalysisSession
{
public:
    std::optional<float> TargetDistance(const Collider& terminal, const glm::vec3& eye,
                                        const glm::vec3& direction,
                                        std::span<const Collider> obstacles) const;
    bool IsFirstTarget(const Collider& terminal, const glm::vec3& eye, const glm::vec3& direction,
                       std::span<const Collider> obstacles, std::optional<float> weaponDistance) const;
    bool Open(bool safe, const Collider& terminal, const glm::vec3& eye, const glm::vec3& direction,
              std::span<const Collider> obstacles, std::optional<float> weaponDistance = {});
    bool Open();
    bool Close();
    void Orbit(float mouseX, float mouseY);
    void Zoom(double wheelSteps);
    glm::vec3 EyePosition() const;
    float FieldOfView() const { return fov; }

    void Reset()
    {
        Close();
    }

    bool Active() const
    {
        return active;
    }

    std::uint64_t Entries() const
    {
        return entries;
    }

    std::uint64_t Exits() const
    {
        return exits;
    }

private:
    bool active{};
    std::uint64_t entries{}, exits{};
    float rotY{35.f};
    float rotX{18.f};
    float fov{45.f};
};
} // namespace aegis::combat
