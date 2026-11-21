#pragma once

#include "gameplay/rendering/atlas_model.h"
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <array>
#include <filesystem>
#include <vector>

namespace aegis
{

// Camera-local viewmodel only; gameplay bullet origin stays in the camera.
class WeaponView
{
public:
    WeaponView() = default;
    WeaponView(const WeaponView&) = delete;
    WeaponView& operator=(const WeaponView&) = delete;
    bool Init(const std::filesystem::path& projectDirectory);
    void Reset();
    void OnShot();
    void Update(double realDeltaTime);
    void Draw(float aspect) const;
    void DrawWorld(const glm::mat4& projection, const glm::mat4& view, const glm::vec3& position,
                   float rotYDegrees = 90) const;
    void Clear();

    int ShotCount() const
    {
        return shotCount;
    }

private:
    void DrawMeshes(const glm::mat4& projection, const glm::mat4& mount, float glow) const;
    AtlasModel atlas;
    float recoil{};
    int shotCount{};
};

} // namespace aegis
