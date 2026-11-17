#pragma once
#include "game_scene.h"
#include "Camera.h"
#include "player_motion.h"
#include <filesystem>

struct GameRunOptions {
    bool combatTest{},smokeTest{},turretTest{},droneTest{true},bossTest{},campaign{true};
    bool combatMapTest{},playerViewTest{};
};

class GameFrameWork {
public:
    int Run(const GameRunOptions& runOptions={});
private:
    bool Init();
    void Update(float deltaTime);
    void Draw();
    void Clear();
    void UpdatePlayerCamera(float deltaTime,bool inputActive);
    void SyncPlayerCamera(bool inputActive);
    void SetCursorCapture(bool captured);
    glm::vec3 CalculateCameraMovement(float deltaTime) const;
    static void CursorPosCallback(GLFWwindow* window,double x,double y);
    static void KeyCallback(GLFWwindow* window,int key,int scan,int action,int mods);
    static void RestartFrameTimeCallback(GLFWwindow* window,int value);

    GLFWwindow* window{};
    aegis::GameScene scene;
    Camera camera;
    double movementTimeScale{0.05};
    float actualTravel{};
    std::uint64_t cameraSpawnRevision{};
    bool cameraSynced{},cursorCaptured{},firstMouse{true};
    double lastMouseX{},lastMouseY{},mouseDeltaX{},mouseDeltaY{};
    GameRunOptions options;
    std::filesystem::path projectDirectory;
    bool glfwReady{},sceneAttempted{};
    bool paused{},resetRequested{},restartFrameTime{},toggleModeRequested{},wasActive{},drawable{};
    int width{},height{},smokeFrame{};
    bool savedEntry{},savedFailure{},savedVictory{},savedPause{},savedBossHud{},savedEmptyHud{},savedDamageEffect{},savedBreakEffect{};
};
