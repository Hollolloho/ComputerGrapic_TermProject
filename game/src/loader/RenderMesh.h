#pragma once

#include "model_loader.h"

#include <GL/glew.h>
#include <vector>

struct MeshBuffer
{
	GLuint vao = 0;
	GLuint vbo = 0;
	GLuint ebo = 0;

	GLsizei indexCount = 0;
	bool Upload(const aegis::ModelMesh& source);
	void Draw() const;
	void Clear();
};

struct RenderMesh : MeshBuffer
{
	glm::vec3 color{1.f};
	glm::vec3 emission{0.f};
	glm::vec3 specular{0.f};
	float shininess = 0.f;
};

bool UploadMeshes(const aegis::ModelData& model, std::vector<RenderMesh>& meshes);
void ClearMeshes(std::vector<RenderMesh>& meshes);
