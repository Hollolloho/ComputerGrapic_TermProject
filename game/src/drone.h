#pragma once

#include "Common.h"
#include "RenderMesh.h"

struct DroneState
{
	glm::vec3 pos{ 0.f };
	glm::vec3 color{ 0.f };
	glm::vec3 rotation{ 0.f };
};

class Drone
{
private:
	std::vector<RenderMesh> meshes;

	GLuint shaderProgram = 0;
	GLint transformLocation = -1;
	GLint colorLocation = -1;
	GLint emissionLocation = -1;

	std::vector<DroneState> drones{};
	glm::mat4 droneMat{ 1.f };
public:
	bool Init();
	void DroneInit();
	void Draw(const glm::mat4& scene);
	void Update(float deltaTime);
	void Clear();
	glm::mat4 CalculateMat(const glm::mat4& scnne, const DroneState& drone);
};
