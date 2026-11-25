// 업로드한 메시를 재질·조명 셰이더로 그리는 공용 렌더러다.
#include "mesh_renderer.h"
#include "loader/Shader.h"

#include <glm/gtc/type_ptr.hpp>

#include <iostream>

namespace aegis
{
bool DrawMesh::Upload(const ModelMesh& mesh, const ModelMaterial& material)
{
    baseColor = material.baseColor;
    metallic = material.metallic;
    roughness = material.roughness;

    return MeshBuffer::Upload(mesh);
}

bool LoadDrawMeshes(const std::filesystem::path& file, std::vector<DrawMesh>& meshes)
{
    ClearDrawMeshes(meshes);
    ModelData model;
    std::string error;
    if (!LoadModel(file, model, error))
    {
        std::cerr << error << '\n';
        return false;
    }
    for (const auto& source : model.meshes)
    {
        auto& draw = meshes.emplace_back();
        if (source.materialIndex >= model.materials.size() || !draw.Upload(source, model.materials[source.materialIndex]))
        {
            ClearDrawMeshes(meshes);
            return false;
        }
    }
    return !meshes.empty();
}

void ClearDrawMeshes(std::vector<DrawMesh>& meshes)
{
    for (auto& mesh : meshes)
        mesh.Clear();
    meshes.clear();
}

bool MeshRenderer::Init(const std::filesystem::path& shaderDirectory)
{
    Clear();
    shaderProgramID = LoadShaderProgram(shaderDirectory / "mesh.vert", shaderDirectory / "mesh.frag");
    if (!shaderProgramID)
    {
        return false;
    }

    transformLocation = glGetUniformLocation(shaderProgramID, "uTransform");
    modelLocation = glGetUniformLocation(shaderProgramID, "uModel");
    baseColorLocation = glGetUniformLocation(shaderProgramID, "uBaseColor");
    metallicLocation = glGetUniformLocation(shaderProgramID, "uMetallic");
    roughnessLocation = glGetUniformLocation(shaderProgramID, "uRoughness");
    specularLocation = glGetUniformLocation(shaderProgramID, "uDielectricF0");
    ambientLocation = glGetUniformLocation(shaderProgramID, "uAmbientStrength");
    eyePositionLocation = glGetUniformLocation(shaderProgramID, "uEyePosition");
    lightDirectionLocation = glGetUniformLocation(shaderProgramID, "uLightDirection");
    lightColorLocation = glGetUniformLocation(shaderProgramID, "uLightColor");
    lightStrengthLocation = glGetUniformLocation(shaderProgramID, "uLightStrength");
    if (transformLocation == -1 || modelLocation == -1 || baseColorLocation == -1 || metallicLocation == -1 ||
        roughnessLocation == -1 || eyePositionLocation == -1 || lightDirectionLocation == -1 ||
        lightColorLocation == -1 || lightStrengthLocation == -1 || specularLocation == -1 ||
        ambientLocation == -1)
    {
        std::cerr << "Required mesh shader uniforms are missing: " << shaderDirectory << '\n';
        return false;
    }
    return true;
}

void MeshRenderer::Begin(const glm::vec3& eyePosition, const glm::vec3& lightDirection,
                         const glm::vec3& lightColor, float lightStrength, float ambientStrength) const
{
    glUseProgram(shaderProgramID);
    glUniform3fv(eyePositionLocation, 1, glm::value_ptr(eyePosition));
    glUniform3fv(lightDirectionLocation, 1, glm::value_ptr(lightDirection));
    glUniform3fv(lightColorLocation, 1, glm::value_ptr(lightColor));
    glUniform1f(lightStrengthLocation, lightStrength);
    glUniform1f(ambientLocation, ambientStrength);
}

void MeshRenderer::Draw(const std::vector<DrawMesh>& meshes, const glm::mat4& scene, const glm::mat4& model,
                        const glm::vec3* overrideColor) const
{
    const glm::mat4 transform = scene * model;
    glUniformMatrix4fv(transformLocation, 1, GL_FALSE, glm::value_ptr(transform));
    glUniformMatrix4fv(modelLocation, 1, GL_FALSE, glm::value_ptr(model));
    for (const auto& mesh : meshes)
    {
        glUniform3fv(baseColorLocation, 1, glm::value_ptr(overrideColor ? *overrideColor : mesh.baseColor));
        glUniform3fv(specularLocation, 1, glm::value_ptr(mesh.dielectricF0));
        glUniform1f(metallicLocation, mesh.metallic);
        glUniform1f(roughnessLocation, mesh.roughness);
        mesh.Draw();
    }
    glBindVertexArray(0);
}

void MeshRenderer::Clear()
{
    if (shaderProgramID)
    {
        glUseProgram(0);
        glDeleteProgram(shaderProgramID);
        shaderProgramID = 0;
    }
}

} // namespace aegis
