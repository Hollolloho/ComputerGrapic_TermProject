#include "FrameWork.h"

#include <GL/glew.h>
#include <glfw/glfw3.h>

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

	window = glfwCreateWindow(960, 540, "AEGIS - OpenGL", nullptr, nullptr);
	if (!window)
		return false;

	glfwMakeContextCurrent(window);
	glewExperimental = GL_TRUE;
	if (glewInit() != GLEW_OK)
		return false;

	glfwSwapInterval(1);
	glClearColor(0.08f, 0.11f, 0.15f, 1.0f);
	return true;
}

void GameFrameWork::Update(float deltaTime)
{
	if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
		glfwSetWindowShouldClose(window, GLFW_TRUE);
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
	glClear(GL_COLOR_BUFFER_BIT);
	glfwSwapBuffers(window);
}

void GameFrameWork::Clear()
{
	if (window)
	{
		glfwDestroyWindow(window);
		window = nullptr;
	}

	glfwTerminate();
}