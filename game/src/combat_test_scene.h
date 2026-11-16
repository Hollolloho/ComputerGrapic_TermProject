#pragma once

#include "combat_range.h"
#include "player_camera.h"
#include "turret_encounter.h"
#include "weapon_view.h"
#include <GL/glew.h>
#include <filesystem>
#include <string>
#include <functional>

struct GLFWwindow;

namespace aegis {

class CombatTestScene {
public:
    bool Init(const std::filesystem::path& projectDirectory);
    void Reset();
    void SampleInput(GLFWwindow* window, bool active);
    void Update(double realDt, double gameDt);
    void Draw(int width, int height,
        const std::function<void(const glm::mat4&,const glm::vec3&)>& drawEnvironment={}) const;
    void Clear();
    void SetSmokeFrame(int frame);
    bool SmokePassed() const;
    bool SaveFrame(const std::filesystem::path& path, int width, int height) const;
    const combat::CombatRange& Range() const { return range; }
    void SetTurretMode(bool enabled);
    void SetCorridorMode(bool enabled);
    bool IsCorridorMode() const { return corridorMode; }
    bool IsTurretMode() const { return turretMode; }
    int SmokeFrameCount() const { return turretMode ? 780 : 300; }
    std::string StatusTitle(bool paused) const;

private:
    bool Fire(const glm::vec3& direction);
    const combat::WeaponInventory& Inventory() const;
    bool Interact(const glm::vec3& direction);
    combat::WeaponInteraction InteractionHint() const;
    void DrawHud(int width,int height) const;
    void DrawBox(const glm::mat4& viewProjection, const glm::vec3& minimum,
        const glm::vec3& maximum, const glm::vec3& color) const;
    combat::CombatRange range;
    combat::TurretEncounter encounter;
    PlayerCamera camera;
    WeaponView weapon;
    GLuint program{};
    GLuint vao{};
    GLuint vbo{};
    GLint transformLocation{-1};
    GLint colorLocation{-1};
    bool mouseHeld{};
    bool coverHeld{};
    bool lookHeld{};
    bool turretHeld{};
    bool dropHeld{};
    bool interactHeld{};
    bool turretMode{};
    bool corridorMode{};
    bool smokeFailureSeen{};
    double lastX{};
    double lastY{};
    float yaw{};
    float pitch{};
};

} // namespace aegis
