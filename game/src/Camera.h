#pragma once

#include "Common.h"

struct Camera
{
    glm::vec3 camPos{ 0.0f };
    glm::vec3 camTarget{ 0.0f };
    glm::vec3 camUp{ 0.0f, 1.0f, 0.0f };

    float fovy = 60.0f;
    float rotX = 0.0f;
    float rotY = 0.0f;
    glm::mat4 view{ 1.0f };
    glm::mat4 projection{ 1.0f };

    void Init();
    void Rotate(float deltaRotX, float deltaRotY);
    void UpdateView();
    glm::vec3 Movement(float deltaTime,bool forward,bool backward,bool left,bool right) const;
    void UpdateProjection(int width, int height);
};
