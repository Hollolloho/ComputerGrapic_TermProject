#pragma once
#include "gameplay/rendering/primitive_view.h"

namespace aegis
{
class DeathView : private PrimitiveView
{
public:
    bool Init(const std::filesystem::path& root);
    void Draw(float progress, float aspect) const;
    using PrimitiveView::Clear;
};
} // namespace aegis
