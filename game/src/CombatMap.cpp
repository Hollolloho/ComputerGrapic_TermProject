#include "CombatMap.h"
#include "Shader.h"
#include "model_loader.h"

#include <iostream>

bool CombatMap::Init()
{
	Clear();

	aegis::ModelData model;
	std::string error;
	// 실행 작업 폴더를 기준으로 한 상대 경로.
	const std::filesystem::path path = "asset/combat_map/obj/combat_map.obj";
	if (!aegis::LoadModel(path, model, error))
	{
		std::cerr << error << '\n';
		return false;
	}

	shaderProgram = LoadShaderProgram("shaders/combat_map.vert", "shaders/combat_map.frag");
	if (!shaderProgram)
		return false;

	transformLocation = glGetUniformLocation(shaderProgram, "transform");
	colorLocation = glGetUniformLocation(shaderProgram, "baseColor");
	emissionLocation = glGetUniformLocation(shaderProgram, "emissionColor");

	if (!UploadMeshes(model, meshes))
	{
		std::cerr << "Could not upload meshes: " << path << '\n';
		Clear();
		return false;
	}
	return true;
}

void CombatMap::Draw(const glm::mat4& scene) const
{
	// 맵 OBJ는 월드 위치로 제작돼 모델 행렬은 단위 행렬이다.
	glUseProgram(shaderProgram);
	glUniformMatrix4fv(transformLocation, 1, GL_FALSE, glm::value_ptr(scene));
	for (const RenderMesh& mesh : meshes)
	{
		glUniform3fv(colorLocation, 1, glm::value_ptr(mesh.color));
		glUniform3fv(emissionLocation, 1, glm::value_ptr(mesh.emission));
		glBindVertexArray(mesh.vao);
		glDrawElements(GL_TRIANGLES, mesh.indexCount, GL_UNSIGNED_INT, nullptr);
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
