#pragma once

#include "main/Common.h"
#include "loader/RenderMesh.h"

class CombatMap
{
public:
	bool Init();
	bool Init(const std::filesystem::path& runtimeRoot);
	void Draw(const glm::mat4& scene, const glm::vec3& eye) const;
	void Clear();

private:
	std::vector<RenderMesh> meshes;
	GLuint shaderProgram = 0;
	GLint transformLocation = -1;
	GLint colorLocation = -1;
	GLint emissionLocation = -1;
	GLint eyeLocation = -1;
	GLint lightPosLocation = -1;
	GLint specularLocation = -1;
	GLint shininessLocation = -1;
};
