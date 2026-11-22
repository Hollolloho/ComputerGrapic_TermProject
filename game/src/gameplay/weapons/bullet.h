#pragma once

#include "gameplay/combat/combat.h"
#include "gameplay/rendering/primitive_view.h"
#include <glm/mat4x4.hpp>

namespace aegis
{
class BulletView : private PrimitiveView
{
public:
    bool Init(const std::filesystem::path& shaderDirectory);
    void Draw(const glm::mat4& projection, const glm::mat4& view, const combat::Bullet& bullet,
              const glm::vec3& color, float radius) const;
    using PrimitiveView::Clear;

private:
    GLint modelViewLocation{-1};
    GLsizei vertexCount{};
};
}
