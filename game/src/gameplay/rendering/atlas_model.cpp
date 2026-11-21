// 보스·무기의 메시와 아틀라스 텍스처를 GPU에 올리고 그린다.
#include "atlas_model.h"
#include "loader/Shader.h"
#include "loader/Texture.h"

#include <glm/gtc/type_ptr.hpp>
#include <iostream>

namespace aegis
{
bool AtlasModel::Init(const std::filesystem::path& root, const std::filesystem::path& runtime,
                      const std::vector<AtlasPart>& parts, const std::array<AtlasTexture, 5>& textureSpecs,
                      float emission)
{
    Clear();
    if (parts.empty())
    {
        return false;
    }
    baseEmission = emission;
    const auto shaders = root / "shaders";
    program = LoadShaderProgram(shaders / "weapon.vert", shaders / "weapon.frag");
    if (!program)
        return false;
    transformLocation = glGetUniformLocation(program, "transform");
    modelLocation = glGetUniformLocation(program, "model");
    shotLocation = glGetUniformLocation(program, "shot");
    if (transformLocation < 0 || modelLocation < 0 || shotLocation < 0)
    {
        Clear();
        return false;
    }
    glUseProgram(program);
    emissionLocation = glGetUniformLocation(program, "emissionMultiplier");
    if (emissionLocation < 0)
    {
        Clear();
        return false;
    }
    glUniform1f(emissionLocation, baseEmission);
    for (std::size_t i = 0; i < textures.size(); ++i)
    {
        const auto& spec = textureSpecs[i];
        textures[i] = LoadPngTexture(runtime / spec.file, spec.srgb, GL_REPEAT);
        if (!textures[i])
        {
            Clear();
            return false;
        }
        const std::string sampler = std::string(spec.name) + "Map";
        const GLint location = glGetUniformLocation(program, sampler.c_str());
        if (location < 0)
        {
            std::cerr << "Missing sampler: " << sampler << '\n';
            Clear();
            return false;
        }
        glUniform1i(location, static_cast<GLint>(i));
    }
    std::size_t totalTriangles = 0;
    for (std::size_t partIndex = 0; partIndex < parts.size(); ++partIndex)
    {
        const auto& part = parts[partIndex];
        ModelData model;
        std::string error;
        if (!LoadModel(runtime / part.mesh, model, error))
        {
            std::cerr << error << '\n';
            Clear();
            return false;
        }
        std::size_t partTriangles = 0;
        for (const auto& mesh : model.meshes)
        {
            if (!mesh.hasUvs || mesh.indices.empty())
            {
                std::cerr << "Weapon mesh has no UV/triangles: " << part.mesh << '\n';
                Clear();
                return false;
            }
            auto& gpu = meshes.emplace_back();
            gpu.partIndex = partIndex;
            if (!gpu.Upload(mesh))
            {
                Clear();
                return false;
            }
            partTriangles += mesh.indices.size() / 3;
        }
        if (partTriangles != static_cast<std::size_t>(part.triangles))
        {
            std::cerr << "Weapon layout is stale: " << part.mesh << '\n';
            Clear();
            return false;
        }
        totalTriangles += partTriangles;
    }
    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glUseProgram(0);
    std::cout << "Atlas model: parts=" << parts.size() << " triangles=" << totalTriangles
              << " textures=" << textures.size() << '\n';
    return true;
}

void AtlasModel::Draw(const glm::mat4& projection, const glm::mat4& view,
                      std::span<const glm::mat4> partMatrices, float glow,
                      std::span<const float> emissionScales) const
{
    if (!program || (!emissionScales.empty() && emissionScales.size() < partMatrices.size()))
    {
        return;
    }
    glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE);
    glUseProgram(program);
    glUniform1f(shotLocation, glow);
    for (std::size_t i = 0; i < textures.size(); ++i)
    {
        glActiveTexture(GL_TEXTURE0 + static_cast<GLenum>(i));
        glBindTexture(GL_TEXTURE_2D, textures[i]);
    }
    for (const auto& mesh : meshes)
    {
        if (mesh.partIndex >= partMatrices.size())
        {
            continue;
        }
        const glm::mat4 model = view * partMatrices[mesh.partIndex];
        glUniform1f(emissionLocation,
                    baseEmission * (emissionScales.empty() ? 1 : emissionScales[mesh.partIndex]));
        const glm::mat4 transform = projection * model;
        glUniformMatrix4fv(modelLocation, 1, GL_FALSE, glm::value_ptr(model));
        glUniformMatrix4fv(transformLocation, 1, GL_FALSE, glm::value_ptr(transform));
        mesh.Draw();
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

void AtlasModel::Clear()
{
    for (auto& mesh : meshes)
        mesh.Clear();
    meshes.clear();
    glDeleteTextures(static_cast<GLsizei>(textures.size()), textures.data());
    textures.fill(0);
    if (program)
    {
        glUseProgram(0);
        glDeleteProgram(program);
        program = 0;
    }
    transformLocation = modelLocation = shotLocation = emissionLocation = -1;
}

} // namespace aegis
