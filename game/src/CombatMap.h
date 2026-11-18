#pragma once

#include "Common.h"
#include "RenderMesh.h"

class CombatMap
{
public:
	bool Init();
	void Draw(const glm::mat4& scene) const;
	void Clear();

private:
	std::vector<RenderMesh> meshes;
	GLuint shaderProgram = 0;
	GLint transformLocation = -1;
	GLint colorLocation = -1;
	GLint emissionLocation = -1;
};
