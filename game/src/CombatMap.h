#pragma once

#include "Common.h"

#include <GL/glew.h>
#include <vector>
#include <filesystem>

class CombatMap
{
public:
	bool Init(const std::filesystem::path& projectRoot);
	void Draw(const glm::mat4& scene) const;
	void Clear();

private:
	struct Mesh
	{
		GLuint vao = 0;
		GLuint vbo = 0;
		GLuint ebo = 0;

		GLsizei indexCount = 0;
		glm::vec3 color{1.f};
		glm::vec3 emission{0.f};
	};

	std::vector<Mesh> meshes;
	GLuint shaderProgram = 0;
	GLint transformLocation = -1;
	GLint colorLocation = -1;
	GLint emissionLocation = -1;
};
