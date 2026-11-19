#pragma once
#include "boss_layout.generated.h"
#include <glm/mat4x4.hpp>

namespace aegis::combat
{
struct BossPose
{
    std::array<glm::mat4, boss_asset::nodeCount> nodes;
    std::array<glm::mat4, boss_asset::partCount> parts;
};

BossPose MakeBossPose(float armorOpen, const glm::vec3& aim);
glm::vec3 BossCoreCenter(const BossPose& pose);
glm::vec3 BossMuzzle(const BossPose& pose, int side);
std::optional<Hit> FindBossHit(const glm::vec3& start, const glm::vec3& end, const BossPose& pose,
                               bool exposed);
} // namespace aegis::combat
