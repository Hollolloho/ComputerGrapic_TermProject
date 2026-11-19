#pragma once
#include "game_scene.h"
#include "game_options.h"
#include "Camera.h"
#include "player_motion.h"
#include <filesystem>

class GameFrameWork
{
public:
    int Run(const GameRunOptions& runOptions = {});

private:
    bool Init();
    void Update(float deltaTime);
    void Draw();
    void Clear();
    void UpdatePlayerCamera(float deltaTime, bool inputActive);
    void SyncPlayerCamera(bool inputActive);
    void SetCursorCapture(bool captured);
    glm::vec3 CalculateCameraMovement(float deltaTime) const;
    static void CursorPosCallback(GLFWwindow* window, double x, double y);
    static void KeyCallback(GLFWwindow* window, int key, int scan, int action, int mods);
    static void RestartFrameTimeCallback(GLFWwindow* window, int value);

    GLFWwindow* window{};
    aegis::GameScene scene;
    Camera camera;
    double movementTimeScale{0.05};
    float actualTravel{};
    std::uint64_t cameraSpawnRevision{};
    bool cameraSynced{};
    bool cursorCaptured{};
    bool firstMouse{true};
    double lastMouseX{};
    double lastMouseY{};
    double mouseDeltaX{};
    double mouseDeltaY{};
    GameRunOptions options;
    std::filesystem::path projectDirectory;
    bool glfwReady{};
    bool sceneAttempted{};
    bool paused{};
    bool resetRequested{};
    bool restartFrameTime{};
    bool toggleModeRequested{};
    bool wasActive{};
    bool drawable{};
    int width{};
    int height{};
    int smokeFrame{};
    bool savedEntry{};
    bool savedFailure{};
    bool savedVictory{};
    bool savedPause{};
    bool savedBossHud{};
    bool savedEmptyHud{};
    bool savedDamageEffect{};
    bool savedBreakEffect{};
};
