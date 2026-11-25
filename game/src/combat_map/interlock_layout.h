#pragma once

#include "gameplay/combat/combat.h"
#include <array>
#include <glm/vec3.hpp>
#include <string>

namespace aegis::interlock_asset
{
const float entranceZ = -18.f;
const float exitZ = -31.f;
const glm::vec3 playerStart(0.f, 0.f, -19.2f);
const glm::vec3 supply(-0.75f, 1.15f, -20.35f);
const glm::vec3 terminal(0.95f, 0.f, -20.35f);
const glm::vec3 exitSign(2.6f, 2.55f, -30.5f);
const glm::vec3 bossMount(0.f, 4.7f, -27.7f);
const float bossScale = .65f;
const float droneScale = .6f;
const float dronePatrolOffset = -1.f;
const std::array<glm::vec3, 4> drones{{
    {-4.2f, 2.6f, -20.6f}, {-2.2f, 2.6f, -22.6f}, {2.2f, 2.6f, -22.6f}, {4.2f, 2.6f, -20.6f}}};

glm::vec3 CoverOffset(const std::string& meshName);
void MoveCover(combat::Collider& collider);
}
