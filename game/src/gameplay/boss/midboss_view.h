#pragma once

#include "midboss.h"
#include "gameplay/rendering/primitive_view.h"

namespace aegis
{
class MidbossView : private PrimitiveView
{
public:
    bool Init(const std::filesystem::path& root);
    void DrawBeam(const glm::mat4& scene, const glm::vec3& eye, const combat::Midboss& boss) const;
    using PrimitiveView::Clear;

private:
    GLint eyeLocation{-1}, sizeLocation{-1}, strengthLocation{-1};
};
} // namespace aegis
