// 보스 부품의 변환 행렬을 만들고 탄환이 장갑·코어 중 어디를 먼저 맞는지 계산한다.
#include "boss_geometry.h"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>

namespace aegis::combat
{
namespace
{
struct Triangle
{
    glm::vec3 a, b, c;
};

const Triangle triangles[] = {
#include "boss_triangles.generated.inc"
};

bool SegmentHitsTriangle(const glm::vec3& from, const glm::vec3& to, const Triangle& t, float& fraction)
{
    const auto d = to - from, e1 = t.b - t.a, e2 = t.c - t.a;
    const auto p = glm::cross(d, e2);
    const float det = glm::dot(e1, p);
    if (std::abs(det) < 1e-8f)
    {
        return false;
    }
    const auto s = from - t.a;
    const float u = glm::dot(s, p) / det;
    if (u < 0 || u > 1)
    {
        return false;
    }
    const auto q = glm::cross(s, e1);
    const float v = glm::dot(d, q) / det;
    if (v < 0 || u + v > 1)
    {
        return false;
    }
    fraction = glm::dot(e2, q) / det;
    return fraction >= 0 && fraction <= 1;
}
} // namespace

BossPose MakeBossPose(float armorOpen, const glm::vec3& aim)
{
    return MakeBossPose(glm::translate(glm::mat4(1), boss_asset::mount), armorOpen, aim);
}

BossPose MakeBossPose(const glm::mat4& root, float armorOpen, const glm::vec3& aim)
{
    BossPose pose;
    for (std::size_t i = 0; i < pose.nodes.size(); ++i)
    {
        const auto& node = boss_asset::nodes[i];
        const auto parent = node.parent < 0 ? root : pose.nodes[node.parent];
        auto offset = node.translation;
        if (i == boss_asset::lower_armor_L_slide)
        {
            offset.x -= boss_asset::armorTravel * armorOpen;
        }
        if (i == boss_asset::lower_armor_R_slide)
        {
            offset.x += boss_asset::armorTravel * armorOpen;
        }
        auto model = glm::translate(parent, offset);
        auto angles = node.rotation;
        if (i == boss_asset::turret_L_rotY || i == boss_asset::turret_R_rotY)
        {
            const auto local = glm::vec3(glm::inverse(parent) * glm::vec4(aim, 1));
            const auto d = local - offset;
            angles.y = std::clamp(glm::degrees(std::atan2(d.x, d.z)), -25.0f, 25.0f);
        }
        if (i == boss_asset::turret_L_rotX || i == boss_asset::turret_R_rotX)
        {
            const auto local = glm::vec3(glm::inverse(parent) * glm::vec4(aim, 1));
            angles.x = std::clamp(
                glm::degrees(std::atan2(-local.y, std::sqrt(local.x * local.x + local.z * local.z))), -10.0f,
                35.0f);
        }
        model = glm::rotate(model, glm::radians(angles.x), {1, 0, 0});
        model = glm::rotate(model, glm::radians(angles.y), {0, 1, 0});
        model = glm::rotate(model, glm::radians(angles.z), {0, 0, 1});
        pose.nodes[i] = model;
    }
    for (std::size_t i = 0; i < pose.parts.size(); ++i)
    {
        pose.parts[i] = pose.nodes[boss_asset::parts[i].node];
    }
    return pose;
}

glm::vec3 BossCoreCenter(const BossPose& pose)
{
    return glm::vec3(pose.nodes[boss_asset::core_visual] * glm::vec4(boss_asset::weakCenter, 1));
}

glm::vec3 BossMuzzle(const BossPose& pose, int side)
{
    return glm::vec3(pose.nodes[side ? boss_asset::muzzle_R : boss_asset::muzzle_L][3]);
}

bool FindBossHit(const glm::vec3& start, const glm::vec3& end, const BossPose& pose,
                 bool exposed, Hit& hit)
{
    const auto inverseCore = glm::inverse(pose.nodes[boss_asset::core_visual]);
    const auto coreStart = glm::vec3(inverseCore * glm::vec4(start, 1));
    const auto coreEnd = glm::vec3(inverseCore * glm::vec4(end, 1));
    const Collider weak{21000, exposed ? HitKind::Body : HitKind::Armor,
                        boss_asset::weakCenter - boss_asset::weakHalf,
                        boss_asset::weakCenter + boss_asset::weakHalf};
    float fraction;
    bool found = SegmentHitsBox(coreStart, coreEnd, weak, fraction);
    if (found)
    {
        hit = {weak.id, weak.kind, fraction, PointOnSegment(start, end, fraction)};
    }
    for (std::size_t i = 0; i < pose.parts.size(); ++i)
    {
        if (i == boss_asset::part_core_crystal)
        {
            continue;
        }
        const auto& part = boss_asset::parts[i];
        const auto inverse = glm::inverse(pose.parts[i]);
        const auto a = glm::vec3(inverse * glm::vec4(start, 1)), b = glm::vec3(inverse * glm::vec4(end, 1));
        const Collider broad{static_cast<ObjectId>(20000 + i), HitKind::Armor, part.low, part.high};
        if (!SegmentHitsBox(a, b, broad, fraction))
        {
            continue;
        }
        for (int index = part.start; index < part.start + part.triangles; ++index)
        {
            if (SegmentHitsTriangle(a, b, triangles[index], fraction) && (!found || fraction <= hit.fraction))
            {
                hit = {broad.id, HitKind::Armor, fraction, start + (end - start) * fraction};
                found = true;
            }
        }
    }
    return found;
}
} // namespace aegis::combat
