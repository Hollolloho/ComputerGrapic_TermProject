// 확정된 파괴·사망을 관찰해 효과를 한 번만 시작한다. 피해나 무기는 생성하지 않는다.
#include "destruction_effects.h"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>

namespace aegis::combat
{
void UpdateShards(std::vector<CrystalShard>& shards, float gameDeltaTime)
{
    if (!std::isfinite(gameDeltaTime) || gameDeltaTime <= 0)
        return;

    const glm::vec3 gravity(0, -3.2f, 0);
    for (std::size_t i = 0; i < shards.size();)
    {
        auto& shard = shards[i];
        shard.age += gameDeltaTime;
        if (shard.age >= shard.lifetime)
        {
            shard = shards.back();
            shards.pop_back();
            continue;
        }
        shard.position += shard.velocity * gameDeltaTime + gravity * (0.5f * gameDeltaTime * gameDeltaTime);
        shard.velocity += gravity * gameDeltaTime;
        ++i;
    }
}

void DestructionEffects::Reset(std::size_t enemyCount)
{
    enemies.assign(enemyCount, {});
    bursts = deaths = 0;
    deathAge = 0;
}

void DestructionEffects::ObserveEnemy(std::size_t slot, bool alive, const glm::mat4& world)
{
    if (slot >= enemies.size())
    {
        return;
    }
    auto& enemy = enemies[slot];
    if (enemy.known && enemy.wasAlive && !alive)
    {
        enemy.debris = {world, 0, true};
        ++bursts;
    }
    enemy.known = true;
    enemy.wasAlive = alive;
}

void DestructionEffects::ObserveDrone(std::size_t slot, bool alive, const glm::mat4& world,
                                       const glm::vec3& direction)
{
    if (slot >= enemies.size()) return;
    const bool destroyed = enemies[slot].known && enemies[slot].wasAlive && !alive;
    ObserveEnemy(slot, alive, world);
    if (!destroyed) return;
    auto& debris = enemies[slot].debris;
    debris.drone = true;
    const float scale = glm::length(glm::vec3(world[0]));
    const glm::mat4 rotation(glm::vec4(glm::vec3(world[0]) / scale, 0),
                             glm::vec4(glm::vec3(world[1]) / scale, 0),
                             glm::vec4(glm::vec3(world[2]) / scale, 0), glm::vec4(0, 0, 0, 1));
    for (std::size_t i = 0; i < debris.pieces.size(); ++i)
    {
        const float angle = static_cast<float>(i) * 2.39996323f;
        auto& piece = debris.pieces[i];
        piece.position = glm::vec3(world * glm::vec4(drone_debris_asset::pieces[i].origin, 1));
        // 실제 피격 방향에 흩어지는 힘을 더한다. 코어 정지 명령은 방향 없이 흩어진다.
        piece.velocity = direction * 2.8f + glm::vec3(std::cos(angle) * 1.6f,
            1.1f + .2f * static_cast<float>(i % 3), std::sin(angle) * 1.6f);
        piece.rotation = rotation;
        piece.spin = 2.5f + .35f * static_cast<float>(i);
        piece.scale = scale;
    }
}

void DestructionEffects::UpdateWorld(float gameDeltaTime)
{
    if (!std::isfinite(gameDeltaTime) || gameDeltaTime <= 0) return;
    for (auto& enemy : enemies)
    {
        auto& item = enemy.debris;
        if (!item.active) continue;
        const float lifetime = item.drone ? DroneDebrisLifetime : DebrisLifetime;
        const float dt = std::min(gameDeltaTime, lifetime - item.age);
        if (item.drone)
        {
            // 바닥 Y=0만 처리한다. 벽·개체·탄환용 Collider에는 파편을 등록하지 않는다.
            // ponytail: 회전 상자 높이로 바닥을 근사한다. 정밀 접촉이 필요하면 메시 지지점으로 교체한다.
            const int steps = std::max(1, static_cast<int>(std::ceil(dt * 120.f)));
            const float step = dt / steps;
            for (int j = 0; j < steps; ++j)
            {
                for (std::size_t i = 0; i < item.pieces.size(); ++i)
                {
                    auto& piece = item.pieces[i];
                    if (piece.alpha <= 0.f) continue;
                    // 첫 착지 뒤에는 다시 튀어도 페이드를 이어 간다. 느린 세계 시간을 쓴다.
                    if (piece.grounded)
                        piece.alpha = std::max(0.f, piece.alpha - step / DroneFadeDuration);
                    const glm::vec3 axis = glm::normalize(glm::vec3(1, .4f + static_cast<float>(i), .7f));
                    piece.rotation = glm::rotate(piece.rotation, piece.spin * step, axis);
                    const auto half = drone_debris_asset::pieces[i].halfSize * piece.scale;
                    const float height = std::abs(piece.rotation[0].y) * half.x +
                        std::abs(piece.rotation[1].y) * half.y + std::abs(piece.rotation[2].y) * half.z;
                    piece.position += piece.velocity * step + glm::vec3(0, -4.9f * step * step, 0);
                    piece.velocity.y -= 9.8f * step;
                    if (piece.position.y <= height)
                    {
                        piece.grounded = true;
                        piece.position.y = height;
                        piece.velocity.y = piece.velocity.y < -1.f ? -piece.velocity.y * .18f : 0.f;
                        piece.velocity.x *= std::exp(-6.f * step);
                        piece.velocity.z *= std::exp(-6.f * step);
                        piece.spin *= std::exp(-8.f * step);
                    }
                }
            }
        }
        item.age = std::min(lifetime, item.age + gameDeltaTime);
        item.active = item.age < lifetime && (!item.drone ||
            std::any_of(item.pieces.begin(), item.pieces.end(), [](const auto& piece) { return piece.alpha > 0.f; }));
    }
}

glm::mat4 DestructionEffects::DronePieceTransform(const EnemyDebris& debris, std::size_t piece)
{
    const auto& fragment = debris.pieces.at(piece);
    return glm::scale(glm::translate(glm::mat4(1), fragment.position) * fragment.rotation,
                      glm::vec3(fragment.scale));
}

void DestructionEffects::ObserveHealth(int health)
{
    if (health <= 0 && !DeathStarted())
    {
        deaths = 1;
        deathAge = 0;
    }
}

void DestructionEffects::UpdateDeath(float realDeltaTime)
{
    if (DeathPlaying() && std::isfinite(realDeltaTime) && realDeltaTime > 0)
    {
        deathAge = std::min(DeathDuration, deathAge + realDeltaTime);
    }
}

glm::mat4 DestructionEffects::PieceTransform(const glm::mat4& world, int piece, float age)
{
    if (!std::isfinite(age))
    {
        age = 0;
    }
    const float t = std::clamp(age, 0.0f, DebrisLifetime), angle = piece * 2.39996323f;
    const glm::vec3 velocity(std::cos(angle) * (1.2f + .2f * piece), 1.4f + .25f * (piece % 3),
                             std::sin(angle) * 1.3f);
    const auto offset = velocity * t + glm::vec3(0, -2.2f * t * t, 0);
    const float scale = std::clamp((DebrisLifetime - t) / .4f, 0.0f, 1.0f);
    return glm::scale(glm::rotate(glm::translate(world, offset), t * (2 + piece),
                                  glm::normalize(glm::vec3(1, .4f + piece, .7f))),
                      glm::vec3(scale));
}
} // namespace aegis::combat
