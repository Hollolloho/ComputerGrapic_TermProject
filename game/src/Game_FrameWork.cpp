#include "FrameWork.h"
#include <GL/glew.h>
#include <glfw/glfw3.h>

namespace
{
	bool moveForward = false;
	bool moveBackward = false;
	bool moveLeft = false;
	bool moveRight = false;
	bool mouseFocused = false;
	bool firstMouse = true;

	double lastMouseX = 0.0;
	double lastMouseY = 0.0;
	double mouseDeltaX = 0.0;
	double mouseDeltaY = 0.0;
}

int GameFrameWork::Run()
{
	if (!Init())
	{
		Clear();
		return 1;
	}

	double previousTime = glfwGetTime();

	while (!glfwWindowShouldClose(window))
	{
		glfwPollEvents();

		double currentTime = glfwGetTime();
		float deltaTime = static_cast<float>(currentTime - previousTime);
		previousTime = currentTime;

		Update(deltaTime);
		Draw();
	}

	Clear();
	return 0;
}

bool GameFrameWork::Init()
{
	if (!glfwInit())
		return false;

	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	window = glfwCreateWindow(1920, 1080, "AEGIS - OpenGL", nullptr, nullptr);
	if (!window)
		return false;

	glfwMakeContextCurrent(window);
	glewExperimental = GL_TRUE;
	if (glewInit() != GLEW_OK)
		return false;

	glfwSwapInterval(1);
	glEnable(GL_DEPTH_TEST);
	glClearColor(0.08f, 0.11f, 0.15f, 1.0f);

	camera.Init();
	if (!combatMap.Init())
		return false;
	if (!drone.Init())
		return false;
	glfwSetWindowUserPointer(window, this);
	glfwSetKeyCallback(window, KeyCallBack);
	glfwSetCursorPosCallback(window, CursorPosCallback);
	glfwSetWindowFocusCallback(window, WindowFocusCallback);
	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
	WindowFocusCallback(window, glfwGetWindowAttrib(window, GLFW_FOCUSED));
	return true;
}

void GameFrameWork::Update(float deltaTime)
{
	// 마우스 이동
	const bool mouseMoved = mouseDeltaX != 0.0 || mouseDeltaY != 0.0;
	if (mouseMoved)
	{
		const float sensitivity = 10.f;
		camera.Rotate(-static_cast<float>(mouseDeltaY) * sensitivity * deltaTime,
			-static_cast<float>(mouseDeltaX) * sensitivity * deltaTime);
		mouseDeltaX = 0.0;
		mouseDeltaY = 0.0;
	}

	// 키보드 이동
	const glm::vec3 movement = CalculateCameraMovement(deltaTime);

	if (!mouseMoved && glm::length(movement) == 0.f)
		return;

	camera.camPos += movement;
	camera.camTarget += movement;
	camera.UpdateView();
}

glm::vec3 GameFrameWork::CalculateCameraMovement(float deltaTime) const
{
	glm::vec3 forward = camera.camTarget - camera.camPos;
	forward.y = 0.f;

	if (glm::length(forward) < 0.001f)
		return glm::vec3(0.f);

	//normalize -> 정규화(방향은 유지, 길이만 1로 만듬), cross -> 외적
	// y축이랑 외적해서 방향을 좌, 우로 변경함
	forward = glm::normalize(forward); 
	const glm::vec3 right = glm::cross(forward, glm::vec3(0.f, 1.f, 0.f));

	// 움직일 방향, forward랑 더하거나 빼서 움직이는 방향을 구함
	glm::vec3 direction(0.f);

	if (moveForward) direction += forward;
	if (moveBackward) direction -= forward;
	if (moveLeft) direction -= right;
	if (moveRight) direction += right;

	if (glm::length(direction) < 0.001f)
		return glm::vec3(0.f);

	const float moveSpeed = 5.f;
	return glm::normalize(direction) * moveSpeed * deltaTime;
}

void GameFrameWork::Draw()
{
	int width = 1920;
	int height = 1080;

	glfwGetFramebufferSize(window, &width, &height);

	if (width <= 0 || height <= 0)
	{
		glfwWaitEventsTimeout(0.1);
		return;
	}

	glViewport(0, 0, width, height);
	camera.UpdateProjection(width, height);

	const glm::mat4 scene = camera.projection * camera.view;
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	combatMap.Draw(scene);
	drone.Draw(scene);
	glfwSwapBuffers(window);
}

void GameFrameWork::Clear()
{
	if (window)
	{
		combatMap.Clear();
		drone.Clear();
		glfwDestroyWindow(window);
		window = nullptr;
	}

	glfwTerminate();
}

void GameFrameWork::CursorPosCallback(GLFWwindow*, double x, double y)
{
	if (!mouseFocused)
		return;

	if (!firstMouse)
	{
		mouseDeltaX += x - lastMouseX;
		mouseDeltaY += y - lastMouseY;
	}
	lastMouseX = x;
	lastMouseY = y;
	firstMouse = false;
}

void GameFrameWork::WindowFocusCallback(GLFWwindow*, int focused)
{
	mouseFocused = focused == GLFW_TRUE;
	firstMouse = true;
	mouseDeltaX = 0.0;
	mouseDeltaY = 0.0;
}

void GameFrameWork::KeyCallBack(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	if (action != GLFW_PRESS && action != GLFW_RELEASE)
		return;

	if (key == GLFW_KEY_Q)
	{
		if (action == GLFW_RELEASE)
			glfwSetWindowShouldClose(window, GLFW_TRUE);
	}
	else if (key == GLFW_KEY_W) moveForward = (action == GLFW_PRESS);
	else if (key == GLFW_KEY_S) moveBackward = (action == GLFW_PRESS);
	else if (key == GLFW_KEY_A) moveLeft = (action == GLFW_PRESS);
	else if (key == GLFW_KEY_D) moveRight = (action == GLFW_PRESS);
}
