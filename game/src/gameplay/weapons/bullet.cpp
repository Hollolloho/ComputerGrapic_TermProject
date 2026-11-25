#include "bullet.h"
#include "gameplay/combat/combat_encounter.h"
#include "loader/Shader.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>
#include <cmath>
#include <iostream>

namespace aegis
{
namespace
{
bool LoadCrystal(const std::filesystem::path& file, std::vector<RenderMesh>& meshes)
{
    ModelData model;
    std::string error;
    if (!LoadModel(file, model, error))
    {
        std::cerr << error << '\n';
        return false;
    }
    return UploadMeshes(model, meshes);
}

bool InitEffectQuad(MeshBuffer& quad)
{
    ModelMesh mesh;
    mesh.vertices = {
        {{-1, 0, 0}, {}, {0, 0}}, {{1, 0, 0}, {}, {1, 0}},
        {{1, 0, 1}, {}, {1, 1}}, {{-1, 0, 1}, {}, {0, 1}}
    };
    mesh.indices = {0, 1, 2, 0, 2, 3};
    return quad.Upload(mesh);
}

// OBJ의 +Z축을 현재 진행 방향과 맞춘다.
glm::mat4 FlightTransform(const glm::vec3& position, const glm::vec3& velocity)
{
    const float rotY = std::atan2(velocity.x, velocity.z);
    const float rotX = -std::atan2(velocity.y, glm::length(glm::vec2(velocity.x, velocity.z)));
    glm::mat4 model = glm::translate(glm::mat4(1), position);
    model = glm::rotate(model, rotY, glm::vec3(0, 1, 0));
    return glm::rotate(model, rotX, glm::vec3(1, 0, 0));
}
}

bool BulletView::Init(const std::filesystem::path& root)
{
    Clear();
    const auto directory = root / "asset/cw01c/runtime/obj";
    program = LoadShaderProgram(root / "shaders/bullet.vert", root / "shaders/bullet.frag");
    if (!program)
        return false;
    transformLocation = glGetUniformLocation(program, "transform");
    modelViewLocation = glGetUniformLocation(program, "modelView");
    colorLocation = glGetUniformLocation(program, "color");
    effectLocation = glGetUniformLocation(program, "effect");
    effectFadeLocation = glGetUniformLocation(program, "effectFade");
    droneShotLocation = glGetUniformLocation(program, "droneShot");
    if (transformLocation < 0 || modelViewLocation < 0 || colorLocation < 0 ||
        effectLocation < 0 || effectFadeLocation < 0 || droneShotLocation < 0 ||
        !LoadCrystal(directory / "crystal_bullet.obj", body) ||
        !LoadCrystal(directory / "crystal_chip.obj", chip) || !InitEffectQuad(effectQuad))
    {
        Clear();
        return false;
    }
    return true;
}

void BulletView::Draw(const glm::mat4& projection, const glm::mat4& view, const combat::Bullet& bullet,
                      const glm::vec3& color, float radius) const
{
    if (!bullet.active)
        return;
    const glm::mat4 model = glm::scale(FlightTransform(bullet.position, bullet.velocity), glm::vec3(radius));
    DrawModel(body, projection, view * model, color);
}

void BulletView::DrawShard(const glm::mat4& projection, const glm::mat4& view, const combat::CrystalShard& shard,
                           const glm::vec3& color) const
{
    glm::mat4 model = FlightTransform(shard.position, shard.velocity);
    model = glm::rotate(model, shard.age * 8.f, glm::vec3(1, 0, 0));
    const float size = shard.size * (1.f - shard.age / shard.lifetime);
    DrawModel(chip, projection, view * glm::scale(model, glm::vec3(size)), color);
}

void BulletView::DrawTrails(const glm::mat4& projection, const glm::mat4& view, const glm::vec3& eye,
                            const combat::CombatEncounter& encounter) const
{
    if (encounter.PlayerFlights().empty() && encounter.EnemyFlights().empty() &&
        encounter.FadingTrails().empty() && encounter.TrailMotes().empty())
        return;

    const glm::vec3 cameraRight(view[0][0], view[1][0], view[2][0]);
    const glm::vec3 cameraUp(view[0][1], view[1][1], view[2][1]);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    glUseProgram(program);
    glUniform1i(effectLocation, 1);

    for (const auto& flight : encounter.PlayerFlights())
    {
        if (flight.bullet.active)
            DrawTrail(projection, view, eye, {flight.bullet.position, flight.bullet.velocity, flight.age, 0.f, flight.droneShot});
    }
    for (const auto& flight : encounter.EnemyFlights())
    {
        if (flight.bullet.active)
            DrawTrail(projection, view, eye, {flight.bullet.position, flight.bullet.velocity, flight.age, 0.f, flight.droneShot});
    }
    for (const auto& trail : encounter.FadingTrails())
        DrawTrail(projection, view, eye, trail);

    glUniform1i(effectLocation, 2);
    for (const auto& mote : encounter.TrailMotes())
    {
        const float fade = 1.f - mote.age / combat::TrailMoteLifetime;
        const float radius = 0.019f * (0.55f + 0.45f * fade);
        const glm::mat4 model(glm::vec4(cameraRight * radius, 0), glm::vec4(0, 1, 0, 0),
                              glm::vec4(cameraUp * (2.f * radius), 0),
                              glm::vec4(mote.position - cameraUp * radius, 1));
        DrawEffect(projection, view * model, fade, fade, mote.droneShot);
    }
    glBindVertexArray(0);
    glUseProgram(0);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
}

void BulletView::DrawTrail(const glm::mat4& projection, const glm::mat4& view, const glm::vec3& eye,
                           const combat::CrystalTrail& trail) const
{
    const float speed = glm::length(trail.velocity);
    if (speed <= 0.f)
        return;
    const float span = std::min({trail.flightTime, combat::TrailLifetime - trail.age, 1.25f / speed});
    if (span <= 0.f)
        return;

    const glm::vec3 distance = trail.velocity * span;
    const glm::vec3 start = trail.end - distance;
    // 이미 지나간 선분에 면을 놓고 관찰자 쪽으로 돌린다.
    const glm::vec3 side = glm::cross(distance, eye - (start + trail.end) * 0.5f);
    const glm::vec3 cameraRight(view[0][0], view[1][0], view[2][0]);
    const glm::vec3 width = 0.022f * (glm::dot(side, side) > 1e-8f ? glm::normalize(side) : cameraRight);
    const glm::mat4 model(glm::vec4(width, 0), glm::vec4(0, 1, 0, 0),
                          glm::vec4(distance, 0), glm::vec4(start, 1));
    DrawEffect(projection, view * model, 1.f - (trail.age + span) / combat::TrailLifetime,
               1.f - trail.age / combat::TrailLifetime, trail.droneShot);
}

void BulletView::DrawEffect(const glm::mat4& projection, const glm::mat4& modelView,
                            float fadeStart, float fadeEnd, bool droneShot) const
{
    glUniformMatrix4fv(transformLocation, 1, GL_FALSE, glm::value_ptr(projection * modelView));
    glUniformMatrix4fv(modelViewLocation, 1, GL_FALSE, glm::value_ptr(modelView));
    glUniform2f(effectFadeLocation, fadeStart, fadeEnd);
    glUniform1i(droneShotLocation, droneShot);
    effectQuad.Draw();
}

void BulletView::DrawModel(const std::vector<RenderMesh>& meshes, const glm::mat4& projection,
                           const glm::mat4& modelView, const glm::vec3& color) const
{
    if (!program)
        return;
    glUseProgram(program);
    glUniform1i(effectLocation, 0);
    glUniformMatrix4fv(transformLocation, 1, GL_FALSE, glm::value_ptr(projection * modelView));
    glUniformMatrix4fv(modelViewLocation, 1, GL_FALSE, glm::value_ptr(modelView));
    glUniform3fv(colorLocation, 1, glm::value_ptr(color));
    for (const auto& mesh : meshes)
        mesh.Draw();
    glBindVertexArray(0);
    glUseProgram(0);
}

void BulletView::Clear()
{
    ClearMeshes(body);
    ClearMeshes(chip);
    if (effectQuad.vao)
        effectQuad.Clear();
    if (program)
        glDeleteProgram(program);
    program = 0;
}
}
