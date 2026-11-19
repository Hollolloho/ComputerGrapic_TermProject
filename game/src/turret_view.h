#pragma once
#include "fixed_turret.h"
#include <filesystem>
#include <GL/glew.h>

namespace aegis
{
class TurretView
{
public:
    bool Init(const std::filesystem::path& root);
    void Draw(const glm::mat4& viewProjection, const combat::FixedTurret& turret) const;
    void DrawDebris(const glm::mat4& viewProjection, const glm::mat4& world, float age) const;
    void Clear();

private:
    void Box(const glm::mat4& viewProjection, const glm::mat4& parent, const glm::vec3& low,
             const glm::vec3& high, const glm::vec3& color) const;
    GLuint program{};
    GLuint vao{};
    GLuint vbo{};
    GLint transformLocation{-1};
    GLint colorLocation{-1};
};
} // namespace aegis
