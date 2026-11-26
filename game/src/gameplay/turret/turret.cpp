#include "turret.h"
#include "gameplay/effects/destruction_effects.h"
#include "loader/Shader.h"
#include "loader/Texture.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <cmath>
#include <iostream>

namespace aegis
{
bool Turret::Init(const std::filesystem::path& root, combat::TurretType type)
{
    Clear();
    program = LoadShaderProgram(root / "shaders/turret_model.vert", root / "shaders/turret_model.frag");
    if (!program)
    {
        return false;
    }
    transformLocation = glGetUniformLocation(program, "uTransform");
    modelLocation = glGetUniformLocation(program, "uModel");
    eyeLocation = glGetUniformLocation(program, "uEye");
    warningLocation = glGetUniformLocation(program, "uWarning");
    colorLocation = glGetUniformLocation(program, "uMaterialColor");
    if (colorLocation < 0 || transformLocation < 0 || modelLocation < 0 || eyeLocation < 0 || warningLocation < 0)
    {
        std::cerr << "Turret shader uniforms are missing\n";
        Clear();
        return false;
    }
    const std::string modelName = type == combat::TurretType::Twin ? "Twin" : "Single_C";
    const auto directory = root / "asset/AEGIS_Turrets" / modelName;
    ModelData data;
    std::string error;
    if (!LoadModel(directory / (modelName + ".obj"), data, error))
    {
        std::cerr << error << '\n';
        Clear();
        return false;
    }
    for (const auto& source : data.meshes)
    {
        if (type == combat::TurretType::SingleC && !source.hasUvs)
        {
            std::cerr << "Single_C mesh has no UV coordinates: " << source.name << '\n';
            Clear();
            return false;
        }
        auto& draw = meshes.emplace_back();
        if (!draw.mesh.Upload(source, data.materials.at(source.materialIndex)))
        {
            Clear();
            return false;
        }
        const auto& name = source.name;
        draw.weapon = name.find("Barrel") != std::string::npos ||
                      name.find("Bore") != std::string::npos || name.find("Mantlet") != std::string::npos;
        const bool rotatesOnX = draw.weapon || name.find("Exposed_core") != std::string::npos;
        const bool base = name.find("Foundation") != std::string::npos ||
                          name.find("Lower_core") != std::string::npos ||
                          name.find("Anchor") != std::string::npos || name.find("Slope") != std::string::npos ||
                          name.find("Upper_base") != std::string::npos || name.find("Bearing") != std::string::npos;
        draw.group = base ? 0 : (rotatesOnX ? 2 : 1);
        draw.piece = base ? 0 : 1;
        if (draw.weapon)
        {
            // Twin의 두 포신은 OBJ X축의 서로 다른 쪽에 있다. 게임 X축은 반대다.
            draw.piece = type == combat::TurretType::Twin && source.vertices.front().position.x < 0 ? 3 : 2;
        }
    }

    glUseProgram(program);
    glUniform1i(glGetUniformLocation(program, "uTextured"), type == combat::TurretType::SingleC);
    glUseProgram(0);
    if (type == combat::TurretType::Twin)
    {
        return !meshes.empty();
    }
    const std::array<const char*, 4> files{"BaseColor.png", "Normal.png", "Metallic.png", "Roughness.png"};
    const std::array<const char*, 4> samplers{"uBaseColorMap", "uNormalMap", "uMetallicMap", "uRoughnessMap"};
    glUseProgram(program);
    for (std::size_t i = 0; i < textures.size(); ++i)
    {
        textures[i] = LoadPngTexture(directory / files[i], i == 0, GL_CLAMP_TO_EDGE);
        const GLint sampler = glGetUniformLocation(program, samplers[i]);
        if (!textures[i] || sampler < 0)
        {
            std::cerr << "Cannot initialize Single_C map: " << files[i] << '\n';
            Clear();
            return false;
        }
        glUniform1i(sampler, static_cast<GLint>(i));
    }
    glBindTexture(GL_TEXTURE_2D, 0);
    glUseProgram(0);
    std::cout << "Single_C: meshes=" << meshes.size() << " textures=" << textures.size() << '\n';
    return !meshes.empty();
}

void Turret::Draw(const glm::mat4& viewProjection, const glm::vec3& eye,
                  const combat::FixedTurret& turret, float debrisAge) const
{
    if (!program || (!turret.Alive() && debrisAge < 0))
    {
        return;
    }
    // 배포 OBJ는 Z 상향, +Y 정면이다. (-x, z, y)로 회전해 게임 축에 맞춘다.
    const auto turn = glm::rotate(glm::mat4(1), glm::radians(180.0f), glm::vec3(0, 1, 0));
    const auto axes = glm::rotate(turn, glm::radians(-90.0f), glm::vec3(1, 0, 0));
    const auto position = glm::translate(glm::mat4(1), turret.Position());
    const auto base = position;
    const auto barrel = turret.BarrelTransform();
    const auto head = turret.HeadTransform();
    const std::array<glm::mat4, 3> transforms{
        base * axes, head * axes, glm::translate(barrel, glm::vec3(0, -1.62f, 0)) * axes};

    std::array<glm::mat4, 4> pieces;
    if (debrisAge >= 0)
    {
        const glm::vec3 centers[] = {
            turret.Position() + glm::vec3(0, .38f, 0),
            turret.Position() + glm::vec3(0, 1.6f, 0),
            glm::vec3(barrel * glm::vec4(turret.BarrelOffset(0), 0, .86f, 1)),
            glm::vec3(barrel * glm::vec4(turret.BarrelOffset(1), 0, .86f, 1))};
        for (int i = 0; i < static_cast<int>(pieces.size()); ++i)
        {
            const auto pivot = glm::translate(glm::mat4(1), centers[i]);
            pieces[i] = glm::translate(combat::DestructionEffects::PieceTransform(pivot, i, debrisAge), -centers[i]);
        }
    }

    glEnable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE);
    glUseProgram(program);
    glUniform3fv(eyeLocation, 1, glm::value_ptr(eye));
    glUniform1f(warningLocation, (turret.Phase() == combat::TurretPhase::Warning ||
        turret.Phase() == combat::TurretPhase::Firing) ? 1.0f : 0.0f);
    for (std::size_t i = 0; i < textures.size(); ++i)
    {
        glActiveTexture(GL_TEXTURE0 + static_cast<GLenum>(i));
        glBindTexture(GL_TEXTURE_2D, textures[i]);
    }
    for (const auto& draw : meshes)
    {
        if (draw.weapon && !turret.HasBarrels())
        {
            continue;
        }
        glUniform3fv(colorLocation, 1, glm::value_ptr(draw.mesh.baseColor));
        auto model = transforms[draw.group];
        if (debrisAge >= 0)
            model = pieces[draw.piece] * model;
        const auto transform = viewProjection * model;
        glUniformMatrix4fv(modelLocation, 1, GL_FALSE, glm::value_ptr(model));
        glUniformMatrix4fv(transformLocation, 1, GL_FALSE, glm::value_ptr(transform));
        draw.mesh.Draw();
    }
    glBindVertexArray(0);
    glUseProgram(0);
    for (std::size_t i = 0; i < textures.size(); ++i)
    {
        glActiveTexture(GL_TEXTURE0 + static_cast<GLenum>(i));
        glBindTexture(GL_TEXTURE_2D, 0);
    }
    glActiveTexture(GL_TEXTURE0);
}

void Turret::Clear()
{
    for (auto& draw : meshes)
    {
        draw.mesh.Clear();
    }
    meshes.clear();
    glDeleteTextures(static_cast<GLsizei>(textures.size()), textures.data());
    textures.fill(0);
    if (program)
    {
        glDeleteProgram(program);
        program = 0;
    }
}
} // namespace aegis
