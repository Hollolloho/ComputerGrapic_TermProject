#pragma once

#include "drone.h"
#include "player_camera.h"
#include "combat_test_scene.h"

#include <array>
#include <filesystem>

namespace aegis {

class GameScene {
public:
    bool Init(const std::filesystem::path& projectDirectory, bool combatTest = false, bool turretTest = false, bool droneTest = true);
    void Update(double deltaTime);
    void Draw(int width, int height) const;
    void Reset();
    void Clear();
    void ToggleMode();
    void SampleInput(GLFWwindow* window, bool active);
    bool IsCombatTest() const { return true; }
    const CombatTestScene& TestScene() const { return testScene; }
    void SmokeFrame(int frame) { testScene.SetSmokeFrame(frame); }

private:
    struct CorridorObject {
        glm::mat4 model{1.0f};
        std::vector<DrawMesh> meshes;
    };

    bool InitCorridor(const std::filesystem::path& projectDirectory);

    MeshRenderer renderer;
    std::array<CorridorObject, 5> corridor;
    Drone drone;
    PlayerCamera camera;
    CombatTestScene testScene;
    bool legacyTest{};
};

} // namespace aegis
