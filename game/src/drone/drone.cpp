#include "drone.h"
#include "loader/Shader.h"
#include "loader/model_loader.h"
#include "gameplay/effects/destruction_effects.h"

#include <cmath>
#include <iostream>

bool Drone::Init()
{
	return Init(".");
}

bool Drone::Init(const std::filesystem::path& runtimeRoot)
{
	Clear();

	aegis::ModelData model;
	std::string error;
	const std::filesystem::path path = runtimeRoot / "asset/drone/obj/drone_assembled.obj";
	if (!aegis::LoadModel(path, model, error))
	{
		std::cerr << error << '\n';
		return false;
	}

	shaderProgram = LoadShaderProgram(runtimeRoot / "shaders/drone.vert", runtimeRoot / "shaders/drone.frag");
	if (!shaderProgram)
		return false;

	transformLocation = glGetUniformLocation(shaderProgram, "transform");
	modelLocation = glGetUniformLocation(shaderProgram, "model");
	colorLocation = glGetUniformLocation(shaderProgram, "baseColor");
	emissionLocation = glGetUniformLocation(shaderProgram, "emissionColor");
	if (transformLocation < 0 || modelLocation < 0 || colorLocation < 0 || emissionLocation < 0)
	{
		Clear();
		return false;
	}

	if (!UploadMeshes(model, meshes))
	{
		std::cerr << "Could not upload meshes: " << path << '\n';
		Clear();
		return false;
	}

	rotors.assign(model.meshes.size(), RotorSide::None);
	parts.assign(model.meshes.size(), Body);

	for (std::size_t i = 0; i < model.meshes.size(); ++i)
	{
		const auto& name = model.meshes[i].name;
		// 조립 OBJ의 이름은 기존 파트 OBJ와 정점을 대조한 그룹이다.
		if (name >= "part_022" && name <= "part_025")
			parts[i] = LeftFan;
		else if (name >= "part_039" && name <= "part_042")
			parts[i] = RightFan;
		else if (name >= "part_048" && name <= "part_054")
			parts[i] = Sensor;
		else if (name >= "part_055" && name <= "part_060")
			parts[i] = Weapon;

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
	drones.clear();
	rotorAngle = 0.f;

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

glm::mat4 Drone::CalculateMat(const DroneState& drone) const
{
	const glm::vec3 rotationAxis(0.f, 1.f, 0.f);
	glm::mat4 model = glm::translate(glm::mat4(1.f), drone.pos);
	model = glm::rotate(model, glm::radians(drone.rotation.y), rotationAxis);

	if (drone.is_turning)
	{
		model = glm::rotate(model, glm::radians(drone.turn_rotate), rotationAxis);
	}

	model = glm::scale(model, glm::vec3(0.5f));
	return model;
}

void Drone::Draw(const glm::mat4& scene) const
{
	for (const DroneState& drone : drones)
		Draw(scene, CalculateMat(drone));
}

void Drone::Draw(const glm::mat4& scene, const glm::mat4& model, bool warning,
	bool armed, bool staticPose, float debrisAge) const
{
	if (!shaderProgram)
		return;
	glUseProgram(shaderProgram);

	for (std::size_t i = 0; i < meshes.size(); ++i)
	{
		const Part part = parts[i];
		if (part == Weapon && (!armed || debrisAge >= 0.f))
			continue;

		const RenderMesh& mesh = meshes[i];
		glm::mat4 partModel = model;
		if (debrisAge >= 0.f)
			partModel = aegis::combat::DestructionEffects::PieceTransform(model, part, debrisAge);

		if (!staticPose && rotors[i] != RotorSide::None)
		{
			const bool isLeft = (rotors[i] == RotorSide::Left);
			const glm::vec3& pivot = isLeft ? leftRotorPivot : rightRotorPivot;
			const float angle = isLeft ? rotorAngle : -rotorAngle;
			partModel = glm::translate(partModel, pivot);
			partModel = glm::rotate(partModel, glm::radians(angle), glm::vec3(0.f, 1.f, 0.f));
			partModel = glm::translate(partModel, -pivot);
		}

		// 실습처럼 화면 변환과 모델 변환을 구분한다. 법선에는 파트 회전까지 적용한다.
		const glm::mat4 transform = scene * partModel;
		glm::vec3 color = mesh.color;
		glm::vec3 emission = mesh.emission;
		if (part == Sensor && warning)
		{
			color = glm::vec3(1.f, 0.04f, 0.02f);
			emission = color * 0.35f;
		}
		glUniformMatrix4fv(transformLocation, 1, GL_FALSE, glm::value_ptr(transform));
		glUniformMatrix4fv(modelLocation, 1, GL_FALSE, glm::value_ptr(partModel));
		glUniform3fv(colorLocation, 1, glm::value_ptr(color));
		glUniform3fv(emissionLocation, 1, glm::value_ptr(emission));
		glBindVertexArray(mesh.vao);
		glDrawElements(GL_TRIANGLES, mesh.indexCount, GL_UNSIGNED_INT, nullptr);
	}

	glBindVertexArray(0);
	glUseProgram(0);
}

void Drone::Update(float deltaTime)
{
	UpdateCombat(deltaTime);
	MovingDrone(deltaTime);
}

void Drone::UpdateCombat(double gameDeltaTime)
{
	if (!std::isfinite(gameDeltaTime) || gameDeltaTime <= 0)
		return;
	// 전투의 위치와 조준은 DroneBehavior가 결정한다. 여기서는 팬만 움직인다.
	rotorAngle = static_cast<float>(std::fmod(rotorAngle + 720.0 * gameDeltaTime, 360.0));
}

void Drone::Reset()
{
	DroneInit();
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
	parts.clear();
	rotors.clear();
	drones.clear();
	if (shaderProgram)
	{
		glUseProgram(0);
		glDeleteProgram(shaderProgram);
		shaderProgram = 0;
	}
	transformLocation = -1;
	modelLocation = -1;
	colorLocation = -1;
	emissionLocation = -1;
}
