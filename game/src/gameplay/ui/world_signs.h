#pragma once
#include "korean_ui.h"
#include <array>

namespace aegis
{
class WorldSigns
{
public:
    bool Init(const std::filesystem::path& shaderDirectory = "shaders");
    void Draw(const glm::mat4& viewProjection, const glm::vec3& supply, const glm::vec3& terminal, bool safe,
              const wchar_t* nextArea, const glm::vec3& exitSign) const;
    void Clear();

private:
    mutable std::array<KoreanUi, 3> signs;
};
} // namespace aegis
