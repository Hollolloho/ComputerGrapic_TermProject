// 적·탄환·무기·파편을 그린다. 충돌과 피해 판정은 여기서 처리하지 않는다.
#include "combat_scene.h"
#include "gameplay/campaign/campaign_environment.h"
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
    const auto drawDrone = [&](const combat::DroneBehavior& state)
    {
        const auto phase = state.Phase();
        if (phase == combat::DronePhase::Destroyed)
            return;
        const bool acquired = phase == combat::DronePhase::Tracking || phase == combat::DronePhase::Warning;
        drone.Draw(viewProjection, state.WorldTransform(), acquired, state.Armed());
    };
    if (IsCoreMode())
    {
        for (const auto& state : core.Drones())
            drawDrone(state);
        for (const auto& turret : core.Turrets())
            twinModel.Draw(viewProjection, PlayerEye(), turret);
        const int magneticStage = core.Phase() == combat::MagneticPhase::Warning ? 1
                                  : core.Phase() == combat::MagneticPhase::Pulse ? 2 : 0;
        coreView.Draw(viewProjection, PlayerEye(), core.CoreAlive(), magneticStage,
                      core.PhaseProgress(), core.Config().radius);
        return;
    }
    if (IsDroneMode() && encounter.EnemyAlive())
        drawDrone(encounter.DroneState());
    if (!IsBossMode())
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

void CombatScene::DrawDroneAimLines(const glm::mat4& viewProjection) const
{
    const auto drawAimLine = [&](const combat::DroneBehavior& state)
    {
        if (state.Phase() == combat::DronePhase::Warning)
        {
            const float reach = glm::mix(0.12f, 1.f, glm::smoothstep(0.f, 0.1f, state.WarningElapsed()));
            drone.DrawAimLine(viewProjection, state.Muzzle(), state.AimPoint(), PlayerEye(), reach);
        }
    };
    if (IsCoreMode())
    {
        for (const auto& state : core.Drones())
            drawAimLine(state);
    }
    else if (IsDroneMode() && encounter.EnemyAlive())
    {
        drawAimLine(encounter.DroneState());
    }
}

void CombatScene::DrawCollisionBoxes(const glm::mat4& viewProjection) const
{
    if (IsCoreMode() || IsBossMode()) return;
    for (const auto& box : encounter.Colliders())
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
    if (!IsDroneMode() && encounter.EnemyAlive())
    {
        DrawBox(viewProjection, {-0.9f, 0, -10.6f}, {0.9f, 0.35f, -9.3f}, {0.25f, 0.3f, 0.4f});

        const auto sensor = encounter.Phase() == combat::TurretPhase::Warning ? glm::vec3(1, 0.15f, 0.1f)
                                                                              : glm::vec3(0.2f, 0.7f, 1);
        DrawBox(viewProjection, {-0.28f, 1.94f, -9.52f}, {0.28f, 2.12f, -9.46f}, sensor);
    }
    // Floor strips make depth and aiming changes visible without extra assets.
    if (!corridorMode)
    {
        for (int z = 0; z >= -12; --z)
        {
            DrawBox(viewProjection, {-5, 0.005f, static_cast<float>(z) - 0.015f},
                    {5, 0.012f, static_cast<float>(z) + 0.015f}, {0.23f, 0.35f, 0.44f});
        }
    }
}

void CombatScene::DrawBullets(const glm::mat4& projection, const glm::mat4& view) const
{
    const glm::vec3 playerColor(0.48f, 0.90f, 1.0f);
    const glm::vec3 enemyColor(1.0f, 0.15f, 0.08f);
    for (const auto& flight : activeBattle->PlayerFlights())
    {
        bulletView.Draw(projection, view, flight.bullet, playerColor, 0.055f);
    }
    for (const auto& flight : activeBattle->EnemyFlights())
    {
        bulletView.Draw(projection, view, flight.bullet, enemyColor, 0.08f);
    }
    for (const auto& shard : activeBattle->ImpactShards())
    {
        bulletView.DrawShard(projection, view, shard, shard.enemy ? enemyColor : playerColor);
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
                       const CampaignEnvironment* environment,
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
    if (IsBossMode())
    {
        bossView.Draw(projection, view, PlayerEye(), boss);
    }
    else if (environment)
    {
        environment->DrawBattle(scene, PlayerEye(), IsCombatMapMode(), DoorProgress());
    }
    if (corridorMode && activeBattle == &encounter)
    {
        terminalRenderer.Begin(PlayerEye(), {0.45f, 0.9f, 0.7f}, {1, 1, 1}, 3, 0.3f);
        terminalRenderer.Draw(terminalMeshes, scene, glm::translate(glm::mat4(1), TerminalPosition()));
    }
    if (combatMapMode && !IsCoreMode())
    {
        signs.Draw(scene, SupplyPosition(), TerminalPosition(), CanAnalyze(), maintenanceMode, approachMode);
    }
    DrawEnemies(scene);
    DrawBullets(projection, view);
    glUseProgram(program);
    glUniform1f(opacityLocation, 1);
    glBindVertexArray(vao);
    DrawCollisionBoxes(scene);
    DrawWeaponFragments(scene);
    const auto supply = IsBossMode() ? boss_asset::supplyWeapon : SupplyPosition();
    if (!IsBossMode())
    {
        DrawBox(scene, supply + glm::vec3(-0.32f, -0.5f, -0.27f),
                supply + glm::vec3(0.32f, -0.15f, 0.27f), {0.1f, 0.55f, 0.38f});
    }
    glBindVertexArray(0);
    glUseProgram(0);
    weapon.DrawWorld(projection, view, supply, IsBossMode() ? 0.0f : 90.0f);
    for (const auto& drop : Inventory().Drops())
    {
        weapon.DrawWorld(projection, view, drop.position);
    }
    // 벽과 월드 물체의 깊이를 사용하고 1인칭 무기를 그리기 전에 조준선을 합성한다.
    DrawDroneAimLines(scene);
    bulletView.DrawTrails(projection, view, PlayerEye(), *activeBattle);
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
