#include "drone.h"
#include "Shader.h"
#include "model_loader.h"

#include <cmath>
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

	rotors.assign(model.meshes.size(), RotorSide::None);

	for (std::size_t i = 0; i < model.meshes.size(); ++i)
	{
		const auto& name = model.meshes[i].name;

		if (name == "part_024" || name == "part_025")
			rotors[i] = RotorSide::Left;
		else if (name == "part_041" || name == "part_042")
			rotors[i] = RotorSide::Right;
	}

	DroneInit();
	return true;
}

void Drone::DroneInit()
{
	// 첫 직선 복도
	drones.push_back({ { 0.65f, 1.95f, -11.40f }, { 1.00f, 1.00f, 1.00f }, { 0.f,  -4.42f, 0.f } });
	drones.back().patrolEndZ = -14.40f;

	// 정비 복도 첫 코너 감지 드론
	drones.push_back({ { 1.56f, 2.30f, -40.69f }, { 1.00f, 0.55f, 0.55f }, { 0.f,  12.38f, 0.f } });

	// 원형 보스방 왼쪽
	drones.push_back({ { 2.55f, 2.30f, -58.80f }, { 0.55f, 1.00f, 0.90f }, { 0.f,  19.95f, 0.f } });
	drones.back().patrolEndZ = -56.8f;

	// 원형 보스방 오른쪽
	drones.push_back({ { 12.50f, 2.40f, -54.00f }, { 0.60f, 0.80f, 1.00f }, { 0.f, -41.82f, 0.f } });
	drones.back().patrolEndZ = -55.8f;

	for (DroneState& drone : drones)
	{
		if (drone.patrolEndZ < drone.patrolStartZ)
		{
			drone.rotation.y += 180.f;
		}
	}
}

glm::mat4 Drone::CalculateMat(const glm::mat4& scene, const DroneState& drone)
{
	const glm::vec3 rotationAxis(0.f, 1.f, 0.f);
	glm::mat4 model = glm::translate(glm::mat4(1.f), drone.pos);
	model = glm::rotate(model, glm::radians(drone.rotation.y), rotationAxis);

	if (drone.is_turning)
	{
		model = glm::rotate(model, glm::radians(drone.turn_rotate), rotationAxis);
	}

	model = glm::scale(model, glm::vec3(0.5f));
	return scene * model;
}

void Drone::Draw(const glm::mat4& scene)
{
	glUseProgram(shaderProgram);

	for (const DroneState& drone : drones)
	{
		droneMat = CalculateMat(scene, drone);

		for (std::size_t i = 0; i < meshes.size(); ++i)
		{
			const RenderMesh& mesh = meshes[i];
			glm::mat4 transform = droneMat;

			if (rotors[i] != RotorSide::None)
			{
				const bool isLeft = (rotors[i] == RotorSide::Left);
				const glm::vec3& pivot = isLeft ? leftRotorPivot : rightRotorPivot;
				const float angle = isLeft ? rotorAngle : -rotorAngle;

				transform = glm::translate(transform, pivot);
				transform = glm::rotate(transform, glm::radians(angle),
					glm::vec3(0.f, 1.f, 0.f));
				transform = glm::translate(transform, -pivot);
			}

			glUniformMatrix4fv(transformLocation, 1, GL_FALSE, glm::value_ptr(transform));
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
	rotorAngle = std::fmod(rotorAngle + 720.f * deltaTime, 360.f);
	MovingDrone(deltaTime);
}

void Drone::MovingDrone(float deltaTime)
{
	const float distanceFrame = 1.2f * deltaTime;
	for (auto& drone : drones)
	{
		if (drone.patrolStartZ == drone.patrolEndZ)
			continue;

		if (drone.is_turning)
		{
			drone.turn_rotate += 60.f * deltaTime;
			if (drone.turn_rotate >= 180.f)
			{
				drone.rotation.y = std::fmod(drone.rotation.y + 180.f, 360.f);
				drone.turn_rotate = 0.f;
				drone.is_turning = false;
				drone.movingToEnd = !drone.movingToEnd;
			}
			continue;
		}

		const float targetZ = drone.movingToEnd ? drone.patrolEndZ : drone.patrolStartZ;
		const float remaining = targetZ - drone.pos.z;

		if (std::abs(remaining) <= distanceFrame)
		{
			drone.pos.z = targetZ;
			drone.is_turning = true;
		}
		else
		{
			drone.pos.z += std::copysign(distanceFrame, remaining);
		}
	}
}

void Drone::Clear()
{
	ClearMeshes(meshes);
	rotors.clear();
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
