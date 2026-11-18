#pragma once

#include "Common.h"

class Drone
{
private:
	struct Mesh
	{
		GLuint vao = 0;
		GLuint vbo = 0;
		GLuint ebo = 0;

		GLsizei indexCount = 0;
		glm::vec3 color{ 1.f };
		glm::vec3 emission{ 0.f };
	};
	glm::mat4 droneMat{ 1.f };
public:
	bool Init();
	void Draw();
	void Update(float deltaTime);
	void Clear();
};