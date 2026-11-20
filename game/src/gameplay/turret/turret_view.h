#pragma once
#include "fixed_turret.h"
#include "gameplay/rendering/primitive_view.h"

namespace aegis
{
class TurretView : private PrimitiveView
{
public:
    bool Init(const std::filesystem::path& root);
    void Draw(const glm::mat4& viewProjection, const combat::FixedTurret& turret) const;
    void DrawDebris(const glm::mat4& viewProjection, const glm::mat4& world, float age) const;
    using PrimitiveView::Clear;

private:
    void Box(const glm::mat4& viewProjection, const glm::mat4& parent, const glm::vec3& low,
             const glm::vec3& high, const glm::vec3& color) const;
};
} // namespace aegis
