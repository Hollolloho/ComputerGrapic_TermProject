#include "CombatMap.h"
#include "loader/Shader.h"
#include "loader/model_loader.h"

#include <iostream>

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
	emergencyLightPosLocation = glGetUniformLocation(shaderProgram, "emergencyLightPos");
	specularLocation = glGetUniformLocation(shaderProgram, "specularColor");
	shininessLocation = glGetUniformLocation(shaderProgram, "shininess");

	if (!UploadMeshes(model, meshes))
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
	// 문틀 위 청회색 등과 왼쪽 벽 비상등의 발광면 앞에 각각 둔다.
	glUniform3f(doorLightPosLocation, 0.f, 3.11f, -17.73f);
	glUniform3f(emergencyLightPosLocation, -2.75f, 3.01f, -15.f);
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
