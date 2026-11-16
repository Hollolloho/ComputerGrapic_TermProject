#pragma once

#include "model_loader.h"

#include <GL/glew.h>
#include <glm/mat4x4.hpp>

namespace aegis {

struct DrawMesh {
    GLuint vao{};
    GLuint vbo{};
    GLuint ebo{};
    GLsizei indexCount{};
    glm::vec3 baseColor{1.0f};
    glm::vec3 dielectricF0{0.04f};
    float metallic{};
    float roughness{0.5f};

    void Upload(const ModelMesh& mesh, const ModelMaterial& material);
    void Clear();
};

class MeshRenderer {
public:
    bool Init(const std::filesystem::path& shaderDirectory);
    void Begin(const glm::vec3& eyePosition, const glm::vec3& lightDirection,
               const glm::vec3& lightColor, float lightStrength, float ambientStrength) const;
    void Draw(const std::vector<DrawMesh>& meshes, const glm::mat4& scene,
              const glm::mat4& model, const glm::vec3* overrideColor=nullptr) const;
    void Clear();

private:
    GLuint shaderProgramID{};
    GLint transformLocation{-1};
    GLint modelLocation{-1};
    GLint baseColorLocation{-1};
    GLint metallicLocation{-1};
    GLint roughnessLocation{-1};
    GLint specularLocation{-1};
    GLint ambientLocation{-1};
    GLint eyePositionLocation{-1};
    GLint lightDirectionLocation{-1};
    GLint lightColorLocation{-1};
    GLint lightStrengthLocation{-1};
};

} // namespace aegis
