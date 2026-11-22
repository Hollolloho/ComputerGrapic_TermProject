#pragma once

#include "main/Common.h"
#include "gameplay/player/player_settings.h"

struct Camera
{
    glm::vec3 camPos{ 0.0f };
    glm::vec3 camTarget{ 0.0f };
    glm::vec3 camUp{ 0.0f, 1.0f, 0.0f };

    float fovy = aegis::player::WorldFovY;
    float rotX = 0.0f;
    float rotY = 0.0f;
    glm::mat4 view{ 1.0f };
    glm::mat4 projection{ 1.0f };

    void Init();
    void Rotate(float deltaRotX, float deltaRotY);
    void UpdateView();
    void UpdateProjection(int width, int height);
};

