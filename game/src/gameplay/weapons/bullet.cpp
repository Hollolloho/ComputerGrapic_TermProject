#include "bullet.h"
#include "gameplay/effects/destruction_effects.h"
#include "loader/Shader.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
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
    if (transformLocation < 0 || modelViewLocation < 0 || colorLocation < 0 ||
        !LoadCrystal(directory / "crystal_bullet.obj", body) || !LoadCrystal(directory / "crystal_chip.obj", chip))
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

void BulletView::DrawModel(const std::vector<RenderMesh>& meshes, const glm::mat4& projection,
                           const glm::mat4& modelView, const glm::vec3& color) const
{
    if (!program)
        return;
    glUseProgram(program);
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
    if (program)
        glDeleteProgram(program);
    program = 0;
}
}
