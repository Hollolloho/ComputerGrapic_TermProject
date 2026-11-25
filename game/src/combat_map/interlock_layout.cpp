#include "interlock_layout.h"

namespace aegis::interlock_asset
{
namespace
{
struct CoverMove
{
    const char* name;
    const char* prefix;
    combat::ObjectId firstCollider, lastCollider;
    glm::vec3 offset;
};

// 본체뿐 아니라 같은 엄폐물의 테두리, 슬랫, 상태등도 함께 옮긴다.
const CoverMove covers[] = {
    {"Cover_B01", "B01_", 40642, 40660, {-.15f, 0, -1.35f}},
    {"Cover_B02", "B02_", 40661, 40679, {-1.95f, 0, 5.1f}},
    {"Cover_B03", "B03_", 40680, 40698, {.27f, 0, 7.65f}}};
}

glm::vec3 CoverOffset(const std::string& meshName)
{
    for (const auto& move : covers)
        if (meshName == move.name || meshName.find(move.prefix) == 0)
            return move.offset;
    return {};
}

void MoveCover(combat::Collider& collider)
{
    for (const auto& move : covers)
    {
        if (collider.id < move.firstCollider || collider.id > move.lastCollider)
            continue;
        collider.minimum += move.offset;
        collider.maximum += move.offset;
        return;
    }
}
}
