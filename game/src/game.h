#pragma once

#include <GL/glew.h>
#include <GLFW/glfw3.h>

struct Cube
{
	GLuint program{};
	GLuint vertexArray{};
	GLuint vertexBuffer{};
	GLuint indexBuffer{};
	GLint transformLocation{};
	GLsizei indexCount{};
};

struct Game
{
	GLFWwindow* window{};
	Cube cube;
};

bool Setup(Game& game);
void Update(GLFWwindow* window);
bool Render(const Game& game);
void Shutdown(Game& game);
