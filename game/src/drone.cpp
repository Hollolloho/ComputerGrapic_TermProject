#include "drone.h"
#include "Shader.h"
#include "model_loader.h"

#include <iostream>

bool Drone::Init()
{
	Clear();

	aegis::ModelData model;
	std::string error;
	const std::filesystem::path path = "asset/drone/obj/drone_assembled.obj";
	if (!aegis::LoadModel(path, model, error))
	{
		std::cerr << error << '\n';
		return false;
	}

	shaderProgram = LoadShaderProgram("shaders/drone.vert", "shaders/drone.frag");
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
	DroneInit();
	return true;
}

void Drone::DroneInit()
{
	// 첫 직선 복도
	drones.push_back({ { 0.65f, 1.95f, -11.40f }, { 1.00f, 1.00f, 1.00f }, { 0.f,  -4.42f, 0.f } });

	// 정비 복도 첫 코너 감지 드론
	drones.push_back({ { 1.56f, 2.30f, -40.69f }, { 1.00f, 0.55f, 0.55f }, { 0.f,  12.38f, 0.f } });

	// 원형 보스방 왼쪽
	drones.push_back({ { 2.55f, 2.30f, -58.80f }, { 0.55f, 1.00f, 0.90f }, { 0.f,  19.95f, 0.f } });

	// 원형 보스방 오른쪽
	drones.push_back({ { 12.50f, 2.40f, -54.00f }, { 0.60f, 0.80f, 1.00f }, { 0.f, -41.82f, 0.f } });
}

glm::mat4 Drone::CalculateMat(const glm::mat4& scene, const DroneState& drone)
{
	glm::mat4 model = glm::translate(glm::mat4(1.f), drone.pos);
	model = glm::rotate(model, glm::radians(drone.rotation.y), glm::vec3(0.f, 1.f, 0.f));
	model = glm::scale(model, glm::vec3(0.8f));
	return scene * model;
}

void Drone::Draw(const glm::mat4& scene)
{
	for (auto& drone : drones)
	{
		droneMat = CalculateMat(scene, drone);
		glUseProgram(shaderProgram);
		glUniformMatrix4fv(transformLocation, 1, GL_FALSE, glm::value_ptr(droneMat));

		for (const RenderMesh& mesh : meshes)
		{
			glUniform3fv(colorLocation, 1, glm::value_ptr(mesh.color));
			glUniform3fv(emissionLocation, 1, glm::value_ptr(mesh.emission));
			glBindVertexArray(mesh.vao);
			glDrawElements(GL_TRIANGLES, mesh.indexCount, GL_UNSIGNED_INT, nullptr);
		}
	}
	glBindVertexArray(0);
}

void Drone::Update(float deltaTime)
{

}

void Drone::Clear()
{
	ClearMeshes(meshes);
	drones.clear();
	if (shaderProgram)
	{
		glUseProgram(0);
		glDeleteProgram(shaderProgram);
		shaderProgram = 0;
	}
	transformLocation = -1;
	colorLocation = -1;
	emissionLocation = -1;
}
