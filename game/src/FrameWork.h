#pragma once

#include "Camera.h"
#include "game_scene.h"
#include "game_options.h"

struct GLFWwindow;

class GameFrameWork
{
public:
	int Run(const GameRunOptions& runOptions = {});

private:
	Camera camera;
	aegis::GameScene scene;
	GameRunOptions options;

	GLFWwindow* window = nullptr;
	bool glfwReady = false;
	bool sceneAttempted = false;
	bool paused = false;
	bool resetRequested = false;
	bool toggleModeRequested = false;
	bool restartFrameTime = true;
	bool wasActive = false;
	bool drawable = false;
	bool cursorCaptured = false;
	bool cameraSynced = false;
	std::uint64_t cameraSpawnRevision = 0;
	double movementTimeScale = 0.05;
	float actualTravel = 0.f;
	int width = 1920;
	int height = 1080;
	int smokeFrame = 0;
	
	bool Init();
	void Update(float deltaTime);
	void Draw();
	void Clear();

	void UpdatePlayerCamera(float deltaTime, bool inputActive);
	void SyncPlayerCamera(bool inputActive);
	void SetCursorCapture(bool captured);
	void UpdateWindowTitle();
	glm::vec3 CalculateCameraMovement(float deltaTime) const;
	static void CursorPosCallback(GLFWwindow* window, double x, double y);
	static void WindowFocusCallback(GLFWwindow* window, int focused);
	static void KeyCallBack(GLFWwindow* window, int key, int scancode, int action, int mods);
};
