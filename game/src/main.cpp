#include "model_loader.h"

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <iostream>
#include <string>

using namespace std;

// ===== 기본 설정과 함수 선언 =====

const int windowWidth = 960;
const int windowHeight = 540;

void errorCallback(int errorCode, const char* message);
int InspectModel(const filesystem::path& path);

// ===== 창 생성과 메인 루프 =====

int main(int argc, char* argv[])
{
	// OBJ 데이터만 확인할 때 사용한다. 인수 없이 실행하면 아래에서 창을 만든다.
	if (argc == 3 && string(argv[1]) == "--inspect-obj")
	{
		return InspectModel(argv[2]);
	}
	if (argc != 1)
	{
		cerr << "Usage: AEGIS.exe [--inspect-obj <model.obj>]\n";
		return 2;
	}

	glfwSetErrorCallback(errorCallback);
	if (!glfwInit())
	{
		return 1;
	}

	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	GLFWwindow* window = glfwCreateWindow(windowWidth, windowHeight, "AEGIS - OpenGL", nullptr, nullptr);
	if (!window)
	{
		glfwTerminate();
		return 1;
	}

	glfwMakeContextCurrent(window);
	glfwSwapInterval(1);
	glewExperimental = GL_TRUE;
	GLenum glewStatus = glewInit();
	if (glewStatus != GLEW_OK)
	{
		cerr << "GLEW: " << glewGetErrorString(glewStatus) << '\n';
		glfwDestroyWindow(window);
		glfwTerminate();
		return 1;
	}

	cout << "OpenGL " << glGetString(GL_VERSION) << endl;
	glClearColor(0.08f, 0.11f, 0.15f, 1.0f);

	while (!glfwWindowShouldClose(window))
	{
		// 입력 처리
		glfwPollEvents();
		if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
		{
			glfwSetWindowShouldClose(window, GLFW_TRUE);
		}

		// 창 크기에 맞춰 화면을 지우고 표시한다.
		int width{}, height{};
		glfwGetFramebufferSize(window, &width, &height);
		if (width > 0 && height > 0)
		{
			glViewport(0, 0, width, height);
			glClear(GL_COLOR_BUFFER_BIT);
			glfwSwapBuffers(window);
		}
		else
		{
			// 최소화된 동안에는 이벤트를 기다린다.
			glfwWaitEventsTimeout(0.1);
		}
	}

	glfwDestroyWindow(window);
	glfwTerminate();
	return 0;
}

// ===== 오류 출력과 OBJ 데이터 확인 =====

void errorCallback(int errorCode, const char* message)
{
	cerr << "GLFW (" << errorCode << "): " << message << '\n';
}

int InspectModel(const filesystem::path& path)
{
	ModelData model;
	string error;
	if (!LoadModel(path, model, error))
	{
		cerr << error << '\n';
		return 1;
	}

	size_t vertices{};
	size_t triangles{};
	for (const ModelMesh& mesh : model.meshes)
	{
		vertices += mesh.vertices.size();
		triangles += mesh.indices.size() / 3;
	}
	cout << "meshes=" << model.meshes.size() << " vertices=" << vertices
		<< " triangles=" << triangles << " materials=" << model.materials.size() << '\n';

	for (const ModelMaterial& material : model.materials)
	{
		cout << material.name << " Kd=" << material.diffuse.r << ',' << material.diffuse.g
			<< ',' << material.diffuse.b << " opacity=" << material.opacity;
		if (!material.diffuseTexture.empty())
		{
			cout << " map_Kd=" << material.diffuseTexture;
		}
		cout << '\n';
	}
	return 0;
}
