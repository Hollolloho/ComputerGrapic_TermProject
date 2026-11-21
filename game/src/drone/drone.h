#pragma once

#include "main/Common.h"
#include "loader/RenderMesh.h"

enum RotorSide
{
	None,
	Left,
	Right
};

class Drone
{
private:
	enum Part
	{
		Body,
		Sensor,
		LeftFan,
		RightFan,
		Weapon
	};

	std::vector<RenderMesh> meshes;
	std::vector<Part> parts;

	GLuint shaderProgram = 0;
	GLint transformLocation = -1;
	GLint modelLocation = -1;
	GLint colorLocation = -1;
	GLint emissionLocation = -1;

	std::vector<RotorSide> rotors{};
	const glm::vec3 leftRotorPivot{ -1.02f, 0.04f, -0.02f };
	const glm::vec3 rightRotorPivot{ 1.02f, 0.04f, -0.02f };
	float rotorAngle = 0.f;
public:
	bool Init();
	bool Init(const std::filesystem::path& runtimeRoot);
	void Draw(const glm::mat4& scene, const glm::mat4& model, bool warning = false,
		bool armed = true, bool staticPose = false, float debrisAge = -1.f) const;
	void UpdateRotor(double gameDeltaTime);
	void Reset();
	void Clear();
};
