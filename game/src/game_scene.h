#pragma once

#include "drone.h"
#include "player_camera.h"
#include "combat_test_scene.h"

#include <filesystem>

namespace aegis {

class GameScene {
public:
    bool Init(const std::filesystem::path& projectDirectory, bool combatTest = false, bool turretTest = false);
    void Update(double deltaTime);
    void Draw(int width, int height) const;
    void Reset();
    void Clear();
    void ToggleMode();
    void SampleInput(GLFWwindow* window, bool active);
    bool IsCombatTest() const { return combatTest; }
    const CombatTestScene& TestScene() const { return testScene; }
    void SmokeFrame(int frame) { testScene.SetSmokeFrame(frame); }

private:
    Drone drone;
    PlayerCamera camera;
    CombatTestScene testScene;
    bool combatTest{};
};

} // namespace aegis
