#pragma once
#include "combat.h"

namespace aegis::combat
{
struct PlayerMove
{
    glm::vec3 eye{}, displacement{};
    bool blocked{};
};

// 시선의 수평 방향과 입력 축으로 이동량을 구한다. 대각선도 같은 거리를 이동한다.
glm::vec3 RequestedPlayerMovement(const glm::vec3& viewDirection, float forwardInput, float rightInput,
                                 float distance);

// Grounded 0.6m-wide, 1.8m-high player; small floor trim is not a wall.
PlayerMove MovePlayer(const glm::vec3& eye, const glm::vec3& desired, std::span<const Collider> walls);
double MovementTimeScale(const glm::vec3& actualDisplacement, double realDeltaTime, double normalSpeed);
} // namespace aegis::combat
