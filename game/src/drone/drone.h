#pragma once

#include "main/Common.h"
#include "loader/RenderMesh.h"

struct DroneState
{
	glm::vec3 pos{ 0.f };
	glm::vec3 color{ 0.f };
	glm::vec3 rotation{ 0.f };
	float patrolStartZ = pos.z;
	float patrolEndZ = pos.z;
	bool movingToEnd = true;
	bool is_turning = false;
	float turn_rotate = 0.f;
};

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
	std::vector<DroneState> drones{};
	const glm::vec3 leftRotorPivot{ -1.02f, 0.04f, -0.02f };
	const glm::vec3 rightRotorPivot{ 1.02f, 0.04f, -0.02f };
	float rotorAngle = 0.f;
public:
	bool Init();
	bool Init(const std::filesystem::path& runtimeRoot);
	void DroneInit();
	void Draw(const glm::mat4& scene) const;
	void Draw(const glm::mat4& scene, const glm::mat4& model, bool warning = false,
		bool armed = true, bool staticPose = false, float debrisAge = -1.f) const;
	void Update(float deltaTime);
	void UpdateCombat(double gameDeltaTime);
	void Reset();
	void MovingDrone(float deltaTime);
	void Clear();
	glm::mat4 CalculateMat(const DroneState& drone) const;
};
