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

	Reset();
	return true;
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

void Drone::UpdateRotor(double gameDeltaTime)
{
	if (!std::isfinite(gameDeltaTime) || gameDeltaTime <= 0)
		return;
	// 전투의 위치와 조준은 DroneBehavior가 결정한다. 여기서는 팬만 움직인다.
	rotorAngle = static_cast<float>(std::fmod(rotorAngle + 720.0 * gameDeltaTime, 360.0));
}

void Drone::Reset()
{
	rotorAngle = 0.f;
}

void Drone::Clear()
{
	ClearMeshes(meshes);
	parts.clear();
	rotors.clear();
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
