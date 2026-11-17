#pragma once
#include "game_scene.h"
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
    static void KeyCallback(GLFWwindow* window,int key,int scan,int action,int mods);
    static void RestartFrameTimeCallback(GLFWwindow* window,int value);

    GLFWwindow* window{};
    aegis::GameScene scene;
    GameRunOptions options;
    std::filesystem::path projectDirectory;
    bool glfwReady{},sceneAttempted{};
    bool paused{},resetRequested{},restartFrameTime{},toggleModeRequested{},wasActive{},drawable{};
    int width{},height{},smokeFrame{};
    bool savedEntry{},savedFailure{},savedVictory{},savedPause{},savedBossHud{},savedEmptyHud{},savedDamageEffect{},savedBreakEffect{};
};
