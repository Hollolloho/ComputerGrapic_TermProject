#pragma once

#include "Common.h"

struct Camera
{
    glm::vec3 camPos{ 0.0f };
    glm::vec3 camTarget{ 0.0f };
    glm::vec3 camUp{ 0.0f, 1.0f, 0.0f };

    float fovy = 60.0f;
    glm::mat4 view{ 1.0f };
    glm::mat4 projection{ 1.0f };

    void Init();
    void UpdateView();
    void UpdateProjection(int width, int height);
};

