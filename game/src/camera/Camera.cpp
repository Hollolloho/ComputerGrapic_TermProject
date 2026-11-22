#include "Camera.h"
#include <algorithm>

void Camera::Init()
{
	camPos = glm::vec3(0.f, aegis::player::EyeHeight, -1.5f);
	camTarget = camPos + glm::vec3(0.f, 0.f, -1.f);
	camUp = glm::vec3(0.f, 1.f, 0.f);
	rotX = 0.f;
	rotY = 0.f;

	UpdateView();
}

void Camera::Rotate(float deltaRotX, float deltaRotY)
{
	rotX = std::clamp(rotX + deltaRotX, -89.f, 89.f);
	rotY += deltaRotY;

	// 시선 벡터에는 오른쪽의 X축 회전부터 적용된다.
	glm::mat4 rotation(1.f);
	rotation = glm::rotate(rotation, glm::radians(rotY), camUp);
	rotation = glm::rotate(rotation, glm::radians(rotX), glm::vec3(1.f, 0.f, 0.f));
	const glm::vec3 forward(rotation * glm::vec4(0.f, 0.f, -1.f, 0.f));
	camTarget = camPos + forward;
}

void Camera::UpdateView()
{
	view = glm::lookAt(camPos, camTarget, camUp);
}

void Camera::UpdateProjection(int width, int height)
{
	if (width <= 0 || height <= 0)	return;

	const float aspect = static_cast<float>(width) / height;

	projection = glm::perspective(glm::radians(fovy), aspect, 0.1f, 100.f);
}
