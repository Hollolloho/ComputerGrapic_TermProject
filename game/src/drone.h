#pragma once

#include "Common.h"
#include "RenderMesh.h"

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
	std::vector<RenderMesh> meshes;

	GLuint shaderProgram = 0;
	GLint transformLocation = -1;
	GLint colorLocation = -1;
	GLint emissionLocation = -1;

	std::vector<RotorSide> rotors{};
	std::vector<DroneState> drones{};
	const glm::vec3 leftRotorPivot{ -1.02f, 0.04f, -0.02f };
	const glm::vec3 rightRotorPivot{ 1.02f, 0.04f, -0.02f };
	float rotorAngle = 0.f;
	glm::mat4 droneMat{ 1.f };
public:
	bool Init();
	void DroneInit();
	void Draw(const glm::mat4& scene);
	void Update(float deltaTime);
	void MovingDrone(float deltaTime);
	void Clear();
	glm::mat4 CalculateMat(const glm::mat4& scnne, const DroneState& drone);
};
