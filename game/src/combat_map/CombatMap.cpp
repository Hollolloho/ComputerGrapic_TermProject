#include "CombatMap.h"
#include "loader/Shader.h"
#include "loader/model_loader.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>

namespace
{
	// 벽의 각 수직 면을 바닥에 투영한 선분. 가장 가까운 면의 법선이 방 안을 향한다.
	struct WallFace
	{
		glm::vec2 start, end;
		glm::vec3 normal;
	};

	std::vector<WallFace> FindWallFaces(const aegis::ModelData& model)
	{
		std::vector<WallFace> walls;
		for (const auto& mesh : model.meshes)
		{
			if (mesh.name.find("wall_solid") == std::string::npos)
				continue;

			for (size_t i = 0; i < mesh.indices.size(); i += 3)
			{
				const auto& a = mesh.vertices[mesh.indices[i]];
				if (std::abs(a.normal.y) > 0.01f)
					continue;
				const auto& b = mesh.vertices[mesh.indices[i + 1]].position;
				const auto& c = mesh.vertices[mesh.indices[i + 2]].position;
				const glm::vec2 start(a.position.x, a.position.z);
				glm::vec2 end(b.x, b.z);
				if (end == start)
					end = glm::vec2(c.x, c.z);
				if (end != start)
					walls.push_back({ start, end, a.normal });
			}
		}
		return walls;
	}

	glm::vec3 EmergencyLightPosition(const aegis::ModelMesh& fixture, const std::vector<WallFace>& walls)
	{
		glm::vec3 low = fixture.vertices.front().position;
		glm::vec3 high = low;
		for (const auto& vertex : fixture.vertices)
		{
			low = glm::min(low, vertex.position);
			high = glm::max(high, vertex.position);
		}
		const glm::vec3 center = (low + high) * 0.5f;
		const glm::vec2 point(center.x, center.z);
		float nearest = std::numeric_limits<float>::max();
		glm::vec3 normal(0.f);
		for (const auto& wall : walls)
		{
			const glm::vec2 edge = wall.end - wall.start;
			const float t = std::clamp(glm::dot(point - wall.start, edge) / glm::dot(edge, edge), 0.f, 1.f);
			const glm::vec2 delta = point - (wall.start + edge * t);
			const float distanceSquared = glm::dot(delta, delta);
			if (distanceSquared < nearest)
			{
				nearest = distanceSquared;
				normal = wall.normal;
			}
		}
		// 첫 복도에서 맞춘 거리만큼 발광 기구를 벽에서 띄워 주변 표면을 비춘다.
		return center + 0.24f * normal;
	}

	bool SetEmergencyLights(GLuint program, const aegis::ModelData& model)
	{
		const auto walls = FindWallFaces(model);
		glUseProgram(program);
		int count = 0;
		for (const auto& mesh : model.meshes)
		{
			if (model.materials[mesh.materialIndex].name != "Steady_emergency_fixture")
				continue;
			const std::string name = "emergencyLightPos[" + std::to_string(count) + "]";
			const GLint location = glGetUniformLocation(program, name.c_str());
			if (location < 0)
			{
				std::cerr << "Missing emergency light uniform: " << name << '\n';
				glUseProgram(0);
				return false;
			}
			const glm::vec3 position = EmergencyLightPosition(mesh, walls);
			glUniform3fv(location, 1, glm::value_ptr(position));
			++count;
		}
		glUniform1i(glGetUniformLocation(program, "emergencyLightCount"), count);
		glUseProgram(0);
		return true;
	}
}

bool CombatMap::Init()
{
	return Init(".");
}

bool CombatMap::Init(const std::filesystem::path& runtimeRoot)
{
	Clear();

	aegis::ModelData model;
	std::string error;
	const std::filesystem::path path = runtimeRoot / "asset/combat_map/obj/combat_map.obj";
	if (!aegis::LoadModel(path, model, error))
	{
		std::cerr << error << '\n';
		return false;
	}

	shaderProgram = LoadShaderProgram(runtimeRoot / "shaders/combat_map.vert", runtimeRoot / "shaders/combat_map.frag");
	if (!shaderProgram)
		return false;

	transformLocation = glGetUniformLocation(shaderProgram, "transform");
	colorLocation = glGetUniformLocation(shaderProgram, "baseColor");
	emissionLocation = glGetUniformLocation(shaderProgram, "emissionColor");
	eyeLocation = glGetUniformLocation(shaderProgram, "eyePos");
	doorLightPosLocation = glGetUniformLocation(shaderProgram, "doorLightPos");
	specularLocation = glGetUniformLocation(shaderProgram, "specularColor");
	shininessLocation = glGetUniformLocation(shaderProgram, "shininess");

	if (!UploadMeshes(model, meshes) || !SetEmergencyLights(shaderProgram, model))
	{
		std::cerr << "Could not upload meshes: " << path << '\n';
		Clear();
		return false;
	}
	return true;
}

void CombatMap::Draw(const glm::mat4& scene, const glm::vec3& eye) const
{
	// 맵 OBJ는 월드 위치로 제작돼 모델 행렬은 단위 행렬이다.
	glUseProgram(shaderProgram);
	glUniformMatrix4fv(transformLocation, 1, GL_FALSE, glm::value_ptr(scene));
	glUniform3fv(eyeLocation, 1, glm::value_ptr(eye));
	// 첫 복도 문틀 위 청회색 등.
	glUniform3f(doorLightPosLocation, 0.f, 3.11f, -17.73f);
	for (const RenderMesh& mesh : meshes)
	{
		glUniform3fv(colorLocation, 1, glm::value_ptr(mesh.color));
		glUniform3fv(emissionLocation, 1, glm::value_ptr(mesh.emission));
		glUniform3fv(specularLocation, 1, glm::value_ptr(mesh.specular));
		glUniform1f(shininessLocation, mesh.shininess);
		mesh.Draw();
	}
	glBindVertexArray(0);
}

void CombatMap::Clear()
{
	ClearMeshes(meshes);

	if (shaderProgram)
	{
		glUseProgram(0);
		glDeleteProgram(shaderProgram);
		shaderProgram = 0;
	}
}
