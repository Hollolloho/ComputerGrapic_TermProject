#include "RenderMesh.h"

#include <cstddef>
#include <limits>

namespace
{
	bool UploadBuffer(GLenum target, GLsizeiptr size, const void* data)
	{
		glBufferData(target, size, data, GL_STATIC_DRAW);

		GLint64 uploadedSize = 0;
		glGetBufferParameteri64v(target, GL_BUFFER_SIZE, &uploadedSize);
		return uploadedSize == size;
	}
}

bool UploadMeshes(const aegis::ModelData& model, std::vector<RenderMesh>& meshes)
{
	ClearMeshes(meshes);
	meshes.reserve(model.meshes.size());
	for (const auto& source : model.meshes)
	{
		if (source.vertices.empty() || source.indices.empty() ||
			source.indices.size() > (std::numeric_limits<GLsizei>::max)() ||
			source.materialIndex >= model.materials.size())
		{
			ClearMeshes(meshes);
			return false;
		}

		RenderMesh& mesh = meshes.emplace_back();
		mesh.indexCount = static_cast<GLsizei>(source.indices.size());
		const auto& material = model.materials[source.materialIndex];
		mesh.color = material.diffuse;
		mesh.emission = material.emissive;

		glGenVertexArrays(1, &mesh.vao);
		glGenBuffers(1, &mesh.vbo);
		glGenBuffers(1, &mesh.ebo);
		if (!mesh.vao || !mesh.vbo || !mesh.ebo)
		{
			ClearMeshes(meshes);
			return false;
		}

		glBindVertexArray(mesh.vao);
		glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo);
		const auto vertexBytes = static_cast<GLsizeiptr>(source.vertices.size() * sizeof(aegis::ModelVertex));
		if (!UploadBuffer(GL_ARRAY_BUFFER, vertexBytes, source.vertices.data()))
		{
			ClearMeshes(meshes);
			return false;
		}

		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(aegis::ModelVertex),
			reinterpret_cast<const void*>(offsetof(aegis::ModelVertex, position)));
		glEnableVertexAttribArray(0);

		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(aegis::ModelVertex),
			reinterpret_cast<const void*>(offsetof(aegis::ModelVertex, normal)));
		glEnableVertexAttribArray(1);

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.ebo);
		const auto indexBytes = static_cast<GLsizeiptr>(source.indices.size() * sizeof(unsigned int));
		if (!UploadBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBytes, source.indices.data()))
		{
			ClearMeshes(meshes);
			return false;
		}
	}

	glBindVertexArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	return !meshes.empty();
}

void ClearMeshes(std::vector<RenderMesh>& meshes)
{
	for (const RenderMesh& mesh : meshes)
	{
		glDeleteVertexArrays(1, &mesh.vao);
		glDeleteBuffers(1, &mesh.vbo);
		glDeleteBuffers(1, &mesh.ebo);
	}
	meshes.clear();
}
