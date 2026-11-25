#include "core_view.h"
#include "loader/Shader.h"
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>
#include <cmath>
#include <iostream>

namespace aegis
{
namespace
{
const glm::vec3 CorePosition(7.8f, 0, -56.0675f);
}

bool CoreView::LoadPart(const std::filesystem::path& path, std::vector<CoreMesh>& meshes,
                        bool transparent)
{
    ModelData model;
    std::string error;
    if (!LoadModel(path, model, error))
    {
        std::cerr << error << '\n';
        return false;
    }
    for (const auto& source : model.meshes)
    {
        const auto& material = model.materials.at(source.materialIndex);
        auto& draw = meshes.emplace_back();
        if (!draw.mesh.Upload(source, material))
            return false;
        draw.emission = glm::max(material.emissive, glm::vec3(0));
        draw.opacity = std::clamp(material.opacity, 0.0f, 1.0f);
        for (const auto& vertex : source.vertices)
        {
            draw.center += vertex.position;
        }
        draw.center /= static_cast<float>(source.vertices.size());
        if (transparent)
        {
            for (std::size_t i = 0; i + 2 < source.indices.size(); i += 3)
            {
                const std::array<unsigned int, 3> indices{
                    source.indices[i], source.indices[i + 1], source.indices[i + 2]};
                const auto center = (source.vertices[indices[0]].position +
                                     source.vertices[indices[1]].position +
                                     source.vertices[indices[2]].position) / 3.0f;
                draw.triangles.push_back({center, indices});
            }
        }
    }
    return !meshes.empty();
}

bool CoreView::Init(const std::filesystem::path& root)
{
    Clear();
    program = LoadShaderProgram(root / "shaders/core.vert", root / "shaders/core.frag");
    if (!program)
    {
        return false;
    }
    transformLocation = glGetUniformLocation(program, "uViewProjection");
    modelLocation = glGetUniformLocation(program, "uModel");
    eyeLocation = glGetUniformLocation(program, "uEye");
    colorLocation = glGetUniformLocation(program, "uBaseColor");
    emissionLocation = glGetUniformLocation(program, "uEmission");
    metallicLocation = glGetUniformLocation(program, "uMetallic");
    roughnessLocation = glGetUniformLocation(program, "uRoughness");
    opacityLocation = glGetUniformLocation(program, "uOpacity");
    layerLocation = glGetUniformLocation(program, "uLayer");
    powerLocation = glGetUniformLocation(program, "uPower");
    if (transformLocation < 0 || modelLocation < 0 || eyeLocation < 0 || colorLocation < 0 ||
        emissionLocation < 0 || metallicLocation < 0 || roughnessLocation < 0 ||
        opacityLocation < 0 || layerLocation < 0 || powerLocation < 0)
    {
        std::cerr << "Core shader uniforms are missing\n";
        Clear();
        return false;
    }
    const auto directory = root / "asset/ai_core/obj";
    if (!LoadPart(directory / "core_structure.obj", structure, false) ||
        !LoadPart(directory / "core_hologram.obj", hologram, false) ||
        !LoadPart(directory / "core_glass.obj", glass, true))
    {
        Clear();
        return false;
    }

    // 바닥 자력장 표시는 작은 고리 메시 하나를 반경에 맞춰 늘린다.
    ModelMesh circle;
    const int segments = 128;
    for (int i = 0; i < segments; ++i)
    {
        const float a = glm::two_pi<float>() * static_cast<float>(i) / segments;
        const float b = glm::two_pi<float>() * static_cast<float>(i + 1) / segments;
        const unsigned int first = static_cast<unsigned int>(circle.vertices.size());
        for (const auto& point : {glm::vec2(std::cos(a), std::sin(a)),
                                  glm::vec2(std::cos(b), std::sin(b))})
        {
            circle.vertices.push_back({{point.x * .97f, .035f, point.y * .97f}, {0, 1, 0}});
            circle.vertices.push_back({{point.x, .035f, point.y}, {0, 1, 0}});
        }
        circle.indices.insert(circle.indices.end(),
                              {first, first + 1, first + 2, first + 2, first + 1, first + 3});
    }
    if (!ring.Upload(circle, ModelMaterial{}))
    {
        Clear();
        return false;
    }
    std::cout << "AI core: structure=" << structure.size() << " hologram=" << hologram.size()
              << " glass=" << glass.size() << '\n';
    return true;
}

void CoreView::DrawPart(const std::vector<CoreMesh>& meshes, const glm::mat4& model, int layer,
                        float power) const
{
    glUniformMatrix4fv(modelLocation, 1, GL_FALSE, glm::value_ptr(model));
    glUniform1i(layerLocation, layer);
    glUniform1f(powerLocation, power);
    for (const auto& draw : meshes)
    {
        glUniform3fv(colorLocation, 1, glm::value_ptr(draw.mesh.baseColor));
        glUniform3fv(emissionLocation, 1, glm::value_ptr(draw.emission));
        glUniform1f(metallicLocation, draw.mesh.metallic);
        glUniform1f(roughnessLocation, draw.mesh.roughness);
        glUniform1f(opacityLocation, draw.opacity);
        draw.mesh.Draw();
    }
}

void CoreView::Draw(const glm::mat4& viewProjection, const glm::vec3& eye, bool coreAlive,
                    int magneticStage, float phaseProgress, float fieldRadius) const
{
    if (!program)
    {
        return;
    }
    const glm::mat4 model = glm::translate(glm::mat4(1), CorePosition);
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);
    glDepthMask(GL_TRUE);
    glUseProgram(program);
    glUniformMatrix4fv(transformLocation, 1, GL_FALSE, glm::value_ptr(viewProjection));
    glUniform3fv(eyeLocation, 1, glm::value_ptr(eye));
    DrawPart(structure, model, 0, coreAlive ? 1.0f : .06f);
    if (coreAlive)
    {
        DrawPart(hologram, model, 1, 1);
    }
    if (coreAlive && magneticStage > 0 && fieldRadius > 0)
    {
        const auto ringModel = glm::scale(model, glm::vec3(fieldRadius, 1, fieldRadius));
        const glm::vec3 color = magneticStage == 1 ? glm::vec3(1, .25f, .025f)
                                                  : glm::vec3(1, .03f, .12f);
        glUniformMatrix4fv(modelLocation, 1, GL_FALSE, glm::value_ptr(ringModel));
        glUniform3fv(colorLocation, 1, glm::value_ptr(color));
        glUniform1i(layerLocation, 3);
        glUniform1f(powerLocation, .6f + .4f * std::clamp(phaseProgress, 0.0f, 1.0f));
        ring.Draw();
    }

    // 불투명 구조와 격자를 먼저 그리고 유리 삼각형은 먼 순서로 합성한다.
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    std::vector<std::size_t> meshOrder(glass.size());
    for (std::size_t i = 0; i < meshOrder.size(); ++i)
        meshOrder[i] = i;
    const auto distance = [&](const glm::vec3& center)
    {
        const auto delta = CorePosition + center - eye;
        return glm::dot(delta, delta);
    };
    std::sort(meshOrder.begin(), meshOrder.end(), [&](std::size_t a, std::size_t b)
              { return distance(glass[a].center) > distance(glass[b].center); });
    glUniformMatrix4fv(modelLocation, 1, GL_FALSE, glm::value_ptr(model));
    glUniform1i(layerLocation, 2);
    glUniform1f(powerLocation, coreAlive ? 1.0f : .1f);
    for (const auto index : meshOrder)
    {
        const auto& draw = glass[index];
        std::vector<std::size_t> order(draw.triangles.size());
        for (std::size_t i = 0; i < order.size(); ++i)
            order[i] = i;
        std::sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b)
                  { return distance(draw.triangles[a].center) > distance(draw.triangles[b].center); });
        std::vector<unsigned int> indices;
        indices.reserve(order.size() * 3);
        for (const auto triangle : order)
        {
            const auto& corners = draw.triangles[triangle].indices;
            indices.insert(indices.end(), corners.begin(), corners.end());
        }
        glUniform3fv(colorLocation, 1, glm::value_ptr(draw.mesh.baseColor));
        glUniform1f(opacityLocation, draw.opacity);
        glBindVertexArray(draw.mesh.vao);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, draw.mesh.ebo);
        glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, indices.size() * sizeof(unsigned int), indices.data());
        glDrawElements(GL_TRIANGLES, draw.mesh.indexCount, GL_UNSIGNED_INT, nullptr);
    }
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glBindVertexArray(0);
    glUseProgram(0);
}

void CoreView::Clear()
{
    for (auto* part : {&structure, &hologram, &glass})
    {
        for (auto& draw : *part)
        {
            draw.mesh.Clear();
        }
        part->clear();
    }
    ring.Clear();
    if (program)
    {
        glDeleteProgram(program);
        program = 0;
    }
}
} // namespace aegis
