// 적·탄환·무기·파편을 그린다. 충돌과 피해 판정은 여기서 처리하지 않는다.
#include "combat_scene.h"
#include "camera/Camera.h"
#include "combat_map/corridor_layout.generated.h"
#include "combat_map/terminal_layout.generated.h"
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>
#include <fstream>
#include <iostream>

namespace aegis
{
// 전투 화면과 분석 화면을 그린다. 피해 판정은 이 파일에서 변경하지 않는다.
void CombatScene::DrawEnemies(const glm::mat4& viewProjection) const
{
    if (coreMode)
    {
        for (const auto& state : core.Drones())
        {
            if (state.Phase() == combat::DronePhase::Destroyed) continue;
            auto world = glm::translate(glm::mat4(1), state.Position());
            world = glm::rotate(world, state.Yaw(), glm::vec3(0, 1, 0));
            world = glm::scale(world, glm::vec3(state.Scale()));
            drone.Draw(viewProjection, world, state.Phase() == combat::DronePhase::Warning, state.Armed());
        }
        for (const auto& turret : core.Turrets())
            twinModel.Draw(viewProjection, PlayerEye(), turret);
        const int magneticStage = core.Phase() == combat::MagneticPhase::Warning ? 1
                                  : core.Phase() == combat::MagneticPhase::Pulse ? 2 : 0;
        coreView.Draw(viewProjection, PlayerEye(), core.CoreAlive(), magneticStage,
                      core.PhaseProgress(), core.Config().radius);
        return;
    }
    if (IsDroneMode() && encounter.EnemyAlive())
    {
        const auto& state = encounter.DroneState();
        // 실습과 같은 순서로 위치 → 회전 → 크기를 적용한다.
        glm::mat4 world = glm::translate(glm::mat4(1), state.Position());
        world = glm::rotate(world, state.Yaw(), glm::vec3(0, 1, 0));
        world = glm::scale(world, glm::vec3(state.Scale()));
        drone.Draw(viewProjection, world, state.Phase() == combat::DronePhase::Warning, state.Armed());
    }
    if (!bossMode)
    {
        for (const auto& turret : encounter.Turrets())
        {
            const auto& model = turret.Type() == combat::TurretType::Twin ? twinModel : singleModel;
            model.Draw(viewProjection, PlayerEye(), turret);
        }
    }
    const auto& droneDebris = destruction.Debris(0);
    if (droneDebris.active)
    {
        drone.Draw(viewProjection, droneDebris.world, false, false, false, droneDebris.age);
    }
    for (std::size_t i = 0; i < encounter.Turrets().size(); ++i)
    {
        const auto& debris = destruction.Debris(static_cast<int>(i + 1));
        if (debris.active)
            turretView.DrawDebris(viewProjection, debris.world, debris.age);
    }
}

void CombatScene::DrawCollisionBoxes(const glm::mat4& viewProjection) const
{
    if (coreMode) return;
    const std::span<const combat::Collider> boxes =
        bossMode ? std::span<const combat::Collider>()
                 : std::span<const combat::Collider>(encounter.Colliders());
    for (const auto& box : boxes)
    {
        if ((box.id >= combat::FixedTurret::BodyId &&
             box.id < combat::FixedTurret::BodyId + 2 * encounter.Turrets().size()) ||
            (corridorMode && box.id == 12000) || (IsDroneMode() && (box.id == 500 || box.id == 501)) ||
            !box.enabled || (corridorMode && box.kind == combat::HitKind::Wall))
        {
            continue;
        }
        const glm::vec3 color = box.id == 500                         ? glm::vec3(0.28f, 0.34f, 0.43f)
                                : box.kind == combat::HitKind::Weapon ? glm::vec3(0.9f, 0.7f, 0.2f)
                                : box.kind == combat::HitKind::Body   ? glm::vec3(0.05f, 0.9f, 0.7f)
                                : box.id == 200                       ? glm::vec3(0.95f, 0.35f, 0.12f)
                                                                      : glm::vec3(0.15f, 0.22f, 0.31f);
        DrawBox(viewProjection, box.minimum, box.maximum, color);
    }
    if (!bossMode && !IsDroneMode() && encounter.EnemyAlive())
    {
        DrawBox(viewProjection, {-0.9f, 0, -10.6f}, {0.9f, 0.35f, -9.3f}, {0.25f, 0.3f, 0.4f});

        const auto sensor = encounter.Phase() == combat::TurretPhase::Warning ? glm::vec3(1, 0.15f, 0.1f)
                                                                              : glm::vec3(0.2f, 0.7f, 1);
        DrawBox(viewProjection, {-0.28f, 1.94f, -9.52f}, {0.28f, 2.12f, -9.46f}, sensor);
    }
    // Floor strips make depth and aiming changes visible without extra assets.
    if (!bossMode && !corridorMode)
    {
        for (int z = 0; z >= -12; --z)
        {
            DrawBox(viewProjection, {-5, 0.005f, static_cast<float>(z) - 0.015f},
                    {5, 0.012f, static_cast<float>(z) + 0.015f}, {0.23f, 0.35f, 0.44f});
        }
    }
}

void CombatScene::DrawBullets(const glm::mat4& viewProjection) const
{
    const auto* battle = ActiveEncounter();
    const auto& shots = battle->PlayerFlights();
    for (const auto& flight : shots)
    {
        const auto p = flight.bullet.position;
        DrawBox(viewProjection, p - glm::vec3(0.055f), p + glm::vec3(0.055f), {1, 0.85f, 0.15f});
    }
    for (const auto& flight : battle->EnemyFlights())
    {
        const auto p = flight.bullet.position;
        DrawBox(viewProjection, p - glm::vec3(0.08f), p + glm::vec3(0.08f), {1, 0.15f, 0.08f});
    }
}

void CombatScene::DrawWeaponFragments(const glm::mat4& viewProjection) const
{
    for (const auto& shard : feedback.Shards())
    {
        const float scale = shard.size * (1 - shard.age / shard.lifetime);
        const auto transform =
            viewProjection *
            glm::rotate(glm::translate(glm::mat4(1), shard.position), shard.size * 93 + shard.age * 9,
                        glm::normalize(glm::vec3(1, 0.6f, 0.3f))) *
            glm::scale(glm::mat4(1), glm::vec3(scale * 2, scale, scale));
        glUniformMatrix4fv(transformLocation, 1, GL_FALSE, glm::value_ptr(transform));
        const glm::vec3 color = shard.size < 0.017f   ? glm::vec3(0.65f, 0.69f, 0.74f)
                                : shard.size < 0.022f ? glm::vec3(0.76f, 0.66f, 0.48f)
                                                      : glm::vec3(0.23f, 0.28f, 0.33f);
        glUniform3fv(colorLocation, 1, glm::value_ptr(color));
        glDrawArrays(GL_TRIANGLES, 0, 36);
    }
}

void CombatScene::DrawBox(const glm::mat4& viewProjection, const glm::vec3& minimum, const glm::vec3& maximum,
                          const glm::vec3& color) const
{
    const auto model =
        glm::scale(glm::translate(glm::mat4(1), (minimum + maximum) * 0.5f), maximum - minimum);
    const auto transform = viewProjection * model;
    glUniformMatrix4fv(transformLocation, 1, GL_FALSE, glm::value_ptr(transform));
    glUniform3fv(colorLocation, 1, glm::value_ptr(color));
    glDrawArrays(GL_TRIANGLES, 0, 36);
}

void CombatScene::Draw(int width, int height,
                       const std::function<void(const glm::mat4&, const glm::vec3&)>& drawEnvironment,
                       bool showHud, const Camera* playerCamera) const
{
    if (width <= 0 || height <= 0)
    {
        return;
    }
    if (analysis.Active())
    {
        DrawAnalysis(width, height);
        return;
    }
    glViewport(0, 0, width, height);
    glDisable(GL_FRAMEBUFFER_SRGB);
    glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glClearColor(0.025f, 0.04f, 0.065f, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    // 실제 플레이는 프레임워크의 카메라 행렬을 그대로 쓴다. 자체 검사는 기존 시점을 유지한다.
    if (DeathStarted())
    {
        playerCamera = nullptr;
    }
    const glm::mat4 projection = playerCamera
        ? playerCamera->projection
        : glm::perspective(glm::radians(CurrentPlayerView().verticalFovDegrees),
                           static_cast<float>(width) / height, 0.05f, 80.0f);
    const glm::mat4 view = playerCamera ? playerCamera->view : PlayerViewMatrix();
    const glm::mat4 scene = projection * view;
    if (bossMode)
    {
        bossView.Draw(projection, view, PlayerEye(), boss);
    }
    else if (drawEnvironment)
    {
        drawEnvironment(scene, PlayerEye());
    }
    if (corridorMode && !bossMode && !coreMode)
    {
        terminalRenderer.Begin(PlayerEye(), {0.45f, 0.9f, 0.7f}, {1, 1, 1}, 3, 0.3f);
        terminalRenderer.Draw(terminalMeshes, scene, glm::translate(glm::mat4(1), TerminalPosition()));
    }
    if (combatMapMode && !coreMode)
    {
        signs.Draw(scene, SupplyPosition(), TerminalPosition(), CanAnalyze(), maintenanceMode, approachMode);
    }
    DrawEnemies(scene);
    glUseProgram(program);
    glUniform1f(opacityLocation, 1);
    glBindVertexArray(vao);
    DrawCollisionBoxes(scene);
    DrawBullets(scene);
    DrawWeaponFragments(scene);
    const auto supply = bossMode ? boss_asset::supplyWeapon : SupplyPosition();
    if (!bossMode)
    {
        DrawBox(scene, supply + glm::vec3(-0.32f, -0.5f, -0.27f),
                supply + glm::vec3(0.32f, -0.15f, 0.27f), {0.1f, 0.55f, 0.38f});
    }
    glBindVertexArray(0);
    glUseProgram(0);
    weapon.DrawWorld(projection, view, supply, bossMode ? 0.0f : 90.0f);
    for (const auto& drop : Inventory().Drops())
    {
        weapon.DrawWorld(projection, view, drop.position);
    }
    if (Inventory().Armed())
    {
        weapon.Draw(static_cast<float>(width) / height);
    }
    // Crisp centre crosshair drawn in framebuffer pixels.
    glEnable(GL_SCISSOR_TEST);
    glClearColor(0.95f, 1, 0.7f, 1);
    glScissor(width / 2 - 8, height / 2 - 1, 17, 3);
    glClear(GL_COLOR_BUFFER_BIT);
    glScissor(width / 2 - 1, height / 2 - 8, 3, 17);
    glClear(GL_COLOR_BUFFER_BIT);
    if (showHud)
    {
        DrawHud(width, height);
    }
    glDisable(GL_SCISSOR_TEST);
    DrawDamageFeedback(width, height);
    if (DeathPlaying())
    {
        deathView.Draw(destruction.DeathProgress(), static_cast<float>(width) / height);
    }
}

void CombatScene::DrawDamageFeedback(int width, int height) const
{
    if (feedback.DamageStrength() <= 0 || width <= 0 || height <= 0)
    {
        return;
    }
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glUseProgram(program);
    glBindVertexArray(vao);
    const glm::vec3 red(1, 0.025f, 0.045f);
    glUniform3fv(colorLocation, 1, glm::value_ptr(red));
    const auto rectangle = [&](float x, float y, float w, float h)
    {
        const auto transform =
            glm::scale(glm::translate(glm::mat4(1), glm::vec3(x, y, 0)), glm::vec3(w, h, 1));
        glUniformMatrix4fv(transformLocation, 1, GL_FALSE, glm::value_ptr(transform));
        glDrawArrays(GL_TRIANGLES, 0, 6);
    };
    for (int band = 0; band < 6; ++band)
    {
        const float inset = static_cast<float>(band) * 0.018f, span = 2 - 2 * inset;
        glUniform1f(opacityLocation, feedback.DamageStrength() * (0.7f - static_cast<float>(band) * 0.1f));
        rectangle(-1 + inset + 0.009f, 0, 0.018f, span);
        rectangle(1 - inset - 0.009f, 0, 0.018f, span);
        rectangle(0, -1 + inset + 0.009f, span - 0.036f, 0.018f);
        rectangle(0, 1 - inset - 0.009f, span - 0.036f, 0.018f);
    }
    glUniform1f(opacityLocation, 1);
    glBindVertexArray(0);
    glUseProgram(0);
    glDisable(GL_BLEND);
    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);
}

void CombatScene::DrawAnalysis(int width, int height) const
{
    glViewport(0, 0, width, height);
    glDisable(GL_FRAMEBUFFER_SRGB);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glDisable(GL_CULL_FACE);
    glClearColor(0.025f, 0.055f, 0.09f, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    const glm::vec3 eye = analysis.EyePosition();
    const auto viewProjection =
        glm::perspective(glm::radians(analysis.FieldOfView()), static_cast<float>(width) / height, 0.05f, 30.0f) *
        glm::lookAt(eye, glm::vec3(0), glm::vec3(0, 1, 0));
    drone.Draw(viewProjection, glm::mat4(1), false, true, true);
    glEnable(GL_SCISSOR_TEST);
    DrawHud(width, height);
    glDisable(GL_SCISSOR_TEST);
}

} // namespace aegis
