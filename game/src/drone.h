#pragma once

#include "Common.h"
#include "RenderMesh.h"

class Drone
{
private:
	std::vector<RenderMesh> meshes;
	GLuint shaderProgram = 0;
	GLint transformLocation = -1;
	GLint colorLocation = -1;
	GLint emissionLocation = -1;

	glm::mat4 droneMat{ 1.f };
public:
	bool Init();
	void Draw();
	void Update(float deltaTime);
	void Clear();
};
