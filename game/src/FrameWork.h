#pragma once

#include "Camera.h"
#include "CombatMap.h"
#include "drone.h"

struct GLFWwindow;

class GameFrameWork
{
public:
	int Run();

private:
	Camera camera;
	CombatMap combatMap;
	Drone drone;

	GLFWwindow* window = nullptr;
	
	bool Init();
	void Update(float deltaTime);
	void Draw();
	void Clear();

	 
	glm::vec3 CalculateCameraMovement(float deltaTime) const;
	static void CursorPosCallback(GLFWwindow* window, double x, double y);
	static void WindowFocusCallback(GLFWwindow* window, int focused);
	static void KeyCallBack(GLFWwindow* window, int key, int scancode, int action, int mods);
};
