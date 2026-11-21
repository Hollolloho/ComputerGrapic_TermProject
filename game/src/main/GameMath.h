#pragma once

#include <glm/vec3.hpp>

namespace aegis
{
glm::vec3 RotateY(const glm::vec3& vector, float angleRadians);
bool IsFinite(const glm::vec3& vector);
}
