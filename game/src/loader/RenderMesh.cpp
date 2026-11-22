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

bool MeshBuffer::Upload(const aegis::ModelMesh& source)
{
	Clear();
	if (source.vertices.empty() || source.indices.empty() ||
		source.indices.size() > (std::numeric_limits<GLsizei>::max)())
		return false;

	glGenVertexArrays(1, &vao);
	glGenBuffers(1, &vbo);
	glGenBuffers(1, &ebo);
	if (!vao || !vbo || !ebo)
	{
		Clear();
		return false;
	}

	glBindVertexArray(vao);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
	const GLsizeiptr vertexBytes = source.vertices.size() * sizeof(aegis::ModelVertex);
	const GLsizeiptr indexBytes = source.indices.size() * sizeof(unsigned int);
	if (!UploadBuffer(GL_ARRAY_BUFFER, vertexBytes, source.vertices.data()) ||
		!UploadBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBytes, source.indices.data()))
	{
		Clear();
		return false;
	}

	// ModelVertex의 위치, 법선, UV를 각 셰이더의 입력 번호 0, 1, 2에 연결한다.
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(aegis::ModelVertex),
		reinterpret_cast<const void*>(offsetof(aegis::ModelVertex, position)));
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(aegis::ModelVertex),
		reinterpret_cast<const void*>(offsetof(aegis::ModelVertex, normal)));
	glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(aegis::ModelVertex),
		reinterpret_cast<const void*>(offsetof(aegis::ModelVertex, uv)));
	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);
	glEnableVertexAttribArray(2);
	glBindVertexArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	indexCount = static_cast<GLsizei>(source.indices.size());
	return true;
}

void MeshBuffer::Draw() const
{
	glBindVertexArray(vao);
	glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, nullptr);
}

void MeshBuffer::Clear()
{
	glDeleteVertexArrays(1, &vao);
	glDeleteBuffers(1, &vbo);
	glDeleteBuffers(1, &ebo);
	vao = vbo = ebo = 0;
	indexCount = 0;
}

bool UploadMeshes(const aegis::ModelData& model, std::vector<RenderMesh>& meshes)
{
	ClearMeshes(meshes);
	meshes.reserve(model.meshes.size());
	for (const auto& source : model.meshes)
	{
		auto& mesh = meshes.emplace_back();
		if (source.materialIndex >= model.materials.size() || !mesh.Upload(source))
		{
			ClearMeshes(meshes);
			return false;
		}
		const auto& material = model.materials[source.materialIndex];
		mesh.color = material.diffuse;
		mesh.emission = material.emissive;
		mesh.specular = material.specular;
		mesh.shininess = material.shininess;
	}
	return !meshes.empty();
}

void ClearMeshes(std::vector<RenderMesh>& meshes)
{
	for (auto& mesh : meshes)
		mesh.Clear();
	meshes.clear();
}
