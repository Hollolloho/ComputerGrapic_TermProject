#pragma once
#include "combat.h"
namespace aegis::combat {
struct PlayerMove {
    glm::vec3 eye{},displacement{};
    bool blocked{};
};
// Grounded 0.6m-wide, 1.8m-high player; small floor trim is not a wall.
PlayerMove MovePlayer(const glm::vec3& eye,const glm::vec3& desired,std::span<const Collider> walls);
double MovementTimeScale(const glm::vec3& actualDisplacement,double realDt);
}
