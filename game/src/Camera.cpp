#include "Camera.h"

void Camera::Init()
{
	camPos = glm::vec3(0.f, 1.65f, -1.5f);
	camTarget = camPos + glm::vec3(0.f, 0.f, -1.f);
	camUp = glm::vec3(0.f, 1.f, 0.f);

	UpdateView();
}

void Camera::UpdateView()
{
	view = glm::lookAt(camPos, camTarget, camUp);
}

void Camera::UpdateProjection(int width, int height)
{
	if (width <= 0 || height <= 0)	return;

	const float aspect = static_cast<float> (width / height);

	projection = glm::perspective(glm::radians(fovy), aspect, 0.1f, 100.f);
}