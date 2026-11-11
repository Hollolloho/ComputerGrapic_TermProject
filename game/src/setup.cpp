#include "game.h"

#include <iostream>
#include <iterator>

constexpr int windowWidth = 960;
constexpr int windowHeight = 540;

static void errorCallback(int errorCode, const char* message)
{
	std::cerr << "GLFW (" << errorCode << "): " << message << '\n';
}

constexpr GLfloat cubeVertices[][4] = {
	// x, y, z, 면 밝기
	{-0.5f, -0.5f,  0.5f, 0.78f}, { 0.5f, -0.5f,  0.5f, 0.78f},
	{ 0.5f,  0.5f,  0.5f, 0.78f}, {-0.5f,  0.5f,  0.5f, 0.78f},
	{ 0.5f, -0.5f, -0.5f, 0.68f}, {-0.5f, -0.5f, -0.5f, 0.68f},
	{-0.5f,  0.5f, -0.5f, 0.68f}, { 0.5f,  0.5f, -0.5f, 0.68f},
	{-0.5f, -0.5f, -0.5f, 0.60f}, {-0.5f, -0.5f,  0.5f, 0.60f},
	{-0.5f,  0.5f,  0.5f, 0.60f}, {-0.5f,  0.5f, -0.5f, 0.60f},
	{ 0.5f, -0.5f,  0.5f, 0.53f}, { 0.5f, -0.5f, -0.5f, 0.53f},
	{ 0.5f,  0.5f, -0.5f, 0.53f}, { 0.5f,  0.5f,  0.5f, 0.53f},
	{-0.5f,  0.5f,  0.5f, 1.00f}, { 0.5f,  0.5f,  0.5f, 1.00f},
	{ 0.5f,  0.5f, -0.5f, 1.00f}, {-0.5f,  0.5f, -0.5f, 1.00f},
	{-0.5f, -0.5f, -0.5f, 0.42f}, { 0.5f, -0.5f, -0.5f, 0.42f},
	{ 0.5f, -0.5f,  0.5f, 0.42f}, {-0.5f, -0.5f,  0.5f, 0.42f},
};

constexpr GLuint cubeIndices[] = {
	0, 1, 2, 0, 2, 3,
	4, 5, 6, 4, 6, 7,
	8, 9, 10, 8, 10, 11,
	12, 13, 14, 12, 14, 15,
	16, 17, 18, 16, 18, 19,
	20, 21, 22, 20, 22, 23,
};

static GLuint CompileShader(GLenum type, const char* source)
{
	GLuint shader = glCreateShader(type);
	glShaderSource(shader, 1, &source, nullptr);
	glCompileShader(shader);

	GLint compiled{};
	glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
	if (!compiled)
	{
		char log[1024]{};
		glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
		std::cerr << "Cube shader: " << log << '\n';
		glDeleteShader(shader);
		return 0;
	}
	return shader;
}

static bool InitCube(Cube& cube)
{
	const char* vertexSource = R"glsl(#version 330 core
layout(location = 0) in vec3 position;
layout(location = 1) in float faceBrightness;
uniform mat4 transform;
out float brightness;

void main()
{
    gl_Position = transform * vec4(position, 1.0);
    brightness = faceBrightness;
}
)glsl";

	const char* fragmentSource = R"glsl(#version 330 core
in float brightness;
out vec4 color;

void main()
{
    color = vec4(vec3(0.25, 0.69, 0.98) * brightness, 1.0);
}
)glsl";

	GLuint vertexShader = CompileShader(GL_VERTEX_SHADER, vertexSource);
	if (!vertexShader)
	{
		return false;
	}
	GLuint fragmentShader = CompileShader(GL_FRAGMENT_SHADER, fragmentSource);
	if (!fragmentShader)
	{
		glDeleteShader(vertexShader);
		return false;
	}

	cube.program = glCreateProgram();
	glAttachShader(cube.program, vertexShader);
	glAttachShader(cube.program, fragmentShader);
	glLinkProgram(cube.program);
	glDeleteShader(vertexShader);
	glDeleteShader(fragmentShader);

	GLint linked{};
	glGetProgramiv(cube.program, GL_LINK_STATUS, &linked);
	if (!linked)
	{
		char log[1024]{};
		glGetProgramInfoLog(cube.program, sizeof(log), nullptr, log);
		std::cerr << "Cube program: " << log << '\n';
		glDeleteProgram(cube.program);
		return false;
	}
	cube.transformLocation = glGetUniformLocation(cube.program, "transform");

	glGenVertexArrays(1, &cube.vertexArray);
	glGenBuffers(1, &cube.vertexBuffer);
	glGenBuffers(1, &cube.indexBuffer);
	glBindVertexArray(cube.vertexArray);
	glBindBuffer(GL_ARRAY_BUFFER, cube.vertexBuffer);
	glBufferData(GL_ARRAY_BUFFER, sizeof(cubeVertices), cubeVertices, GL_STATIC_DRAW);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cube.indexBuffer);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(cubeIndices), cubeIndices, GL_STATIC_DRAW);

	constexpr GLsizei stride = sizeof(cubeVertices[0]);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, nullptr);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<const void*>(3 * sizeof(GLfloat)));
	glEnableVertexAttribArray(1);
	glBindVertexArray(0);
	cube.indexCount = static_cast<GLsizei>(std::size(cubeIndices));
	return true;
}

bool Setup(Game& game)
{
	glfwSetErrorCallback(errorCallback);
	if (!glfwInit())
	{
		return false;
	}

	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	GLFWwindow* window = glfwCreateWindow(windowWidth, windowHeight, "AEGIS - OpenGL", nullptr, nullptr);
	if (!window)
	{
		glfwTerminate();
		return false;
	}

	glfwMakeContextCurrent(window);
	glfwSwapInterval(1);
	glewExperimental = GL_TRUE;
	GLenum glewStatus = glewInit();
	if (glewStatus != GLEW_OK)
	{
		std::cerr << "GLEW: " << glewGetErrorString(glewStatus) << '\n';
		glfwDestroyWindow(window);
		glfwTerminate();
		return false;
	}

	std::cout << "OpenGL " << glGetString(GL_VERSION) << std::endl;
	glClearColor(0.08f, 0.11f, 0.15f, 1.0f);
	glEnable(GL_DEPTH_TEST);

	if (!InitCube(game.cube))
	{
		glfwDestroyWindow(window);
		glfwTerminate();
		return false;
	}

	game.window = window;
	return true;
}

void Shutdown(Game& game)
{
	glDeleteVertexArrays(1, &game.cube.vertexArray);
	glDeleteBuffers(1, &game.cube.vertexBuffer);
	glDeleteBuffers(1, &game.cube.indexBuffer);
	glDeleteProgram(game.cube.program);
	glfwDestroyWindow(game.window);
	glfwTerminate();
}
