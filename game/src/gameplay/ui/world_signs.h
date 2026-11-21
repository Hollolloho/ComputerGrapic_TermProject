#pragma once
#include "korean_ui.h"
#include <array>

namespace aegis
{
class WorldSigns
{
public:
    bool Init();
    void Draw(const glm::mat4& viewProjection, const glm::vec3& supply, const glm::vec3& terminal, bool safe,
              bool maintenance, bool coreApproach) const;
    void Clear();

private:
    mutable std::array<KoreanUi, 3> signs;
};
} // namespace aegis
