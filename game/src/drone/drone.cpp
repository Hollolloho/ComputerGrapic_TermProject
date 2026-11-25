#include "drone.h"
#include "loader/Shader.h"
#include "loader/model_loader.h"
#include "gameplay/effects/destruction_effects.h"

#include <cmath>
#include <iostream>

namespace
{
	// 월드 패스의 깊이를 읽고 색만 합성한 뒤, 깊이 쓰기와 불투명 상태로 돌아간다.
	struct AimBlend
	{
		AimBlend()
		{
			glDepthMask(GL_FALSE);
			glEnable(GL_BLEND);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		}

		~AimBlend()
		{
			glDisable(GL_BLEND);
			glDepthMask(GL_TRUE);
		}
	};

	bool InitAimBeam(MeshBuffer& beam)
	{
		// 셰이더가 이 부피 안에서 원형 단면의 레이저를 그린다.
		aegis::ModelMesh mesh;
		mesh.vertices = {
			{{-1, -1, 0}}, {{1, -1, 0}}, {{1, 1, 0}}, {{-1, 1, 0}},
			{{-1, -1, 1}}, {{1, -1, 1}}, {{1, 1, 1}}, {{-1, 1, 1}}
		};
		mesh.indices = {
			0, 2, 1, 0, 3, 2, 4, 5, 6, 4, 6, 7,
			0, 1, 5, 0, 5, 4, 1, 2, 6, 1, 6, 5,
			2, 3, 7, 2, 7, 6, 3, 0, 4, 3, 4, 7
		};
		return beam.Upload(mesh);
	}
}

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
	opacityLocation = glGetUniformLocation(shaderProgram, "opacity");
	beamLocation = glGetUniformLocation(shaderProgram, "drawBeam");
	eyeLocation = glGetUniformLocation(shaderProgram, "eyePosition");
	beamEyeLocation = glGetUniformLocation(shaderProgram, "beamEye");
	if (transformLocation < 0 || modelLocation < 0 || colorLocation < 0 || emissionLocation < 0 ||
		opacityLocation < 0 || beamLocation < 0 || eyeLocation < 0 || beamEyeLocation < 0)
	{
		Clear();
		return false;
	}

	if (!UploadMeshes(model, meshes) || !InitAimBeam(aimBeam))
	{
		std::cerr << "Could not upload meshes: " << path << '\n';
		Clear();
		return false;
	}

	rotors.assign(model.meshes.size(), RotorSide::None);
	parts.assign(model.meshes.size(), Body);

	for (std::size_t i = 0; i < model.meshes.size(); ++i)
	{
		const auto& mesh = model.meshes[i];
		const auto& name = mesh.name;
		// 조립 OBJ의 이름은 기존 파트 OBJ와 정점을 대조한 그룹이다.
		if (name >= "part_022" && name <= "part_025")
			parts[i] = LeftFan;
		else if (name >= "part_039" && name <= "part_042")
			parts[i] = RightFan;
		else if (name >= "part_048" && name <= "part_054")
			parts[i] = Sensor;
		else if (name >= "part_055" && name <= "part_060")
			parts[i] = Weapon;

		// 050은 붉은 테두리, 051은 렌즈, 052~054는 렌즈 안의 중앙 표식이다.
		const auto& material = model.materials[mesh.materialIndex].name;
		if (parts[i] == Sensor && (material == "lens" ||
			(material == "red" && name >= "part_052" && name <= "part_054")))
			eyeMeshes.push_back(i);

		if (name == "part_024" || name == "part_025")
			rotors[i] = RotorSide::Left;
		else if (name == "part_041" || name == "part_042")
			rotors[i] = RotorSide::Right;
	}

	Reset();
	return true;
}

void Drone::Draw(const glm::mat4& scene, const glm::mat4& model, bool targetAcquired,
	bool armed, bool staticPose, float debrisAge) const
{
	if (!shaderProgram)
		return;
	glUseProgram(shaderProgram);
	glUniform1i(beamLocation, GL_FALSE);
	glUniform1f(opacityLocation, 1.f);

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
		glUniformMatrix4fv(transformLocation, 1, GL_FALSE, glm::value_ptr(transform));
		glUniformMatrix4fv(modelLocation, 1, GL_FALSE, glm::value_ptr(partModel));
		glUniform3fv(colorLocation, 1, glm::value_ptr(mesh.color));
		glUniform3fv(emissionLocation, 1, glm::value_ptr(mesh.emission));
		mesh.Draw();
	}

	if (targetAcquired && armed && !staticPose && debrisAge < 0.f && !eyeMeshes.empty())
	{
		const AimBlend blend;
		glDepthFunc(GL_LEQUAL);
		const glm::mat4 transform = scene * model;
		glUniformMatrix4fv(transformLocation, 1, GL_FALSE, glm::value_ptr(transform));
		glUniformMatrix4fv(modelLocation, 1, GL_FALSE, glm::value_ptr(model));
		glUniform1f(opacityLocation, 0.40f);
		for (const auto i : eyeMeshes)
		{
			const auto& mesh = meshes[i];
			glUniform3fv(colorLocation, 1, glm::value_ptr(mesh.color));
			glUniform3fv(emissionLocation, 1, glm::value_ptr(mesh.emission + glm::vec3(0.34f, 0.052f, 0.04f)));
			mesh.Draw();
		}
		glDepthFunc(GL_LESS);
	}

	glBindVertexArray(0);
	glUseProgram(0);
}

void Drone::DrawAimLine(const glm::mat4& scene, const glm::vec3& origin, const glm::vec3& target,
	const glm::vec3& viewer, float reach) const
{
	const glm::vec3 offset = target - origin;
	const float distance = glm::length(offset);
	const float length = distance * glm::clamp(reach, 0.f, 1.f);
	if (length <= 0.001f)
		return;
	const glm::vec3 axis = offset / distance;
	const glm::vec3 basis = std::abs(axis.y) < 0.99f ? glm::vec3(0, 1, 0) : glm::vec3(1, 0, 0);
	const glm::vec3 right = glm::normalize(glm::cross(basis, axis));
	const glm::vec3 up = glm::cross(axis, right);

	const glm::vec3 eyeOffset = viewer - origin;
	const float eyeAlong = glm::dot(eyeOffset, axis);
	const float radius = 0.023f;
	const glm::vec3 beamEye(glm::dot(eyeOffset, right) / radius, glm::dot(eyeOffset, up) / radius,
		eyeAlong / length);
	const glm::mat4 model(glm::vec4(right * radius, 0), glm::vec4(up * radius, 0),
		glm::vec4(axis * length, 0), glm::vec4(origin, 1));
	const glm::mat4 transform = scene * model;
	const AimBlend blend;
	glUseProgram(shaderProgram);
	glUniformMatrix4fv(transformLocation, 1, GL_FALSE, glm::value_ptr(transform));
	glUniformMatrix4fv(modelLocation, 1, GL_FALSE, glm::value_ptr(model));
	glUniform3fv(eyeLocation, 1, glm::value_ptr(viewer));
	glUniform3fv(beamEyeLocation, 1, glm::value_ptr(beamEye));
	glUniform1i(beamLocation, GL_TRUE);
	aimBeam.Draw();
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
	eyeMeshes.clear();
	if (aimBeam.vao)
		aimBeam.Clear();
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
	opacityLocation = -1;
	beamLocation = -1;
	eyeLocation = -1;
	beamEyeLocation = -1;
}
