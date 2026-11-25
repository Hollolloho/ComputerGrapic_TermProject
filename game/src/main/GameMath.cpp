#include "GameMath.h"
#include <cmath>

namespace aegis
{
glm::vec3 RotateY(const glm::vec3& vector, float angleRadians)
{
    const float cosine = std::cos(angleRadians);
    const float sine = std::sin(angleRadians);
    return {cosine * vector.x + sine * vector.z, vector.y, -sine * vector.x + cosine * vector.z};
}

bool IsFinite(const glm::vec3& vector)
{
    return std::isfinite(vector.x) && std::isfinite(vector.y) && std::isfinite(vector.z);
}
}
