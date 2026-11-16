#include "mesh_renderer.h"

#include <glm/gtc/type_ptr.hpp>

#include <cstddef>
#include <fstream>
#include <iostream>
#include <sstream>

namespace aegis {
namespace {

GLuint CompileShader(GLenum shaderType, const std::filesystem::path& path)
{
    std::ifstream file(path);
    std::ostringstream sourceStream;
    if (file) {
        sourceStream << file.rdbuf();
    }
    if (!file || !sourceStream) {
        std::cerr << "Could not read shader: " << path << '\n';
        return 0;
    }

    const std::string source = sourceStream.str();
    const char* code = source.c_str();
    // 파일에서 읽은 GLSL 코드를 OpenGL에서 컴파일한다.
    const GLuint shader = glCreateShader(shaderType);
    glShaderSource(shader, 1, &code, nullptr);
    glCompileShader(shader);

    GLint compiled{};
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (!compiled) {
        GLint logLength{};
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);
        std::string log(logLength, '\0');
        glGetShaderInfoLog(shader, logLength, nullptr, log.data());
        std::cerr << "Shader compile failed: " << path << '\n' << log.c_str() << '\n';
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

GLuint MakeShaderProgram(const std::filesystem::path& shaderDirectory)
{
    const auto vertexPath = shaderDirectory / "drone.vert";
    const auto fragmentPath = shaderDirectory / "drone.frag";
    const GLuint vertexShader = CompileShader(GL_VERTEX_SHADER, vertexPath);
    if (!vertexShader) {
        return 0;
    }
    const GLuint fragmentShader = CompileShader(GL_FRAGMENT_SHADER, fragmentPath);
    if (!fragmentShader) {
        glDeleteShader(vertexShader);
        return 0;
    }

    const GLuint shaderProgramID = glCreateProgram();
    glAttachShader(shaderProgramID, vertexShader);
    glAttachShader(shaderProgramID, fragmentShader);
    glLinkProgram(shaderProgramID);
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    GLint linked{};
    glGetProgramiv(shaderProgramID, GL_LINK_STATUS, &linked);
    if (!linked) {
        GLint logLength{};
        glGetProgramiv(shaderProgramID, GL_INFO_LOG_LENGTH, &logLength);
        std::string log(logLength, '\0');
        glGetProgramInfoLog(shaderProgramID, logLength, nullptr, log.data());
        std::cerr << "Shader link failed: " << vertexPath << " and " << fragmentPath
                  << '\n' << log.c_str() << '\n';
        glDeleteProgram(shaderProgramID);
        return 0;
    }
    return shaderProgramID;
}

} // namespace

void DrawMesh::Upload(const ModelMesh& mesh, const ModelMaterial& material)
{
    baseColor = material.baseColor;
    metallic = material.metallic;
    roughness = material.roughness;

    // VAO에 정점 속성과 인덱스 버퍼 연결을 기록한다.
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glGenBuffers(1, &ebo);
    glBindVertexArray(vao);

    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, mesh.vertices.size() * sizeof(ModelVertex),
                 mesh.vertices.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, mesh.indices.size() * sizeof(unsigned int),
                 mesh.indices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(ModelVertex),
                          reinterpret_cast<const void*>(offsetof(ModelVertex, position)));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(ModelVertex),
                          reinterpret_cast<const void*>(offsetof(ModelVertex, normal)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    indexCount = static_cast<GLsizei>(mesh.indices.size());
}

void DrawMesh::Clear()
{
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    glDeleteBuffers(1, &ebo);
    vao = vbo = ebo = 0;
    indexCount = 0;
}

bool MeshRenderer::Init(const std::filesystem::path& shaderDirectory)
{
    Clear();
    shaderProgramID = MakeShaderProgram(shaderDirectory);
    if (!shaderProgramID) {
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
    if (transformLocation == -1 || modelLocation == -1 || baseColorLocation == -1 ||
        metallicLocation == -1 || roughnessLocation == -1 || eyePositionLocation == -1 ||
        lightDirectionLocation == -1 || lightColorLocation == -1 || lightStrengthLocation == -1 ||
        specularLocation == -1 || ambientLocation == -1) {
        std::cerr << "Required mesh shader uniforms are missing: " << shaderDirectory << '\n';
        return false;
    }
    return true;
}

void MeshRenderer::Begin(const glm::vec3& eyePosition, const glm::vec3& lightDirection,
                         const glm::vec3& lightColor, float lightStrength,
                         float ambientStrength) const
{
    glUseProgram(shaderProgramID);
    glUniform3fv(eyePositionLocation, 1, glm::value_ptr(eyePosition));
    glUniform3fv(lightDirectionLocation, 1, glm::value_ptr(lightDirection));
    glUniform3fv(lightColorLocation, 1, glm::value_ptr(lightColor));
    glUniform1f(lightStrengthLocation, lightStrength);
    glUniform1f(ambientLocation, ambientStrength);
}

void MeshRenderer::Draw(const std::vector<DrawMesh>& meshes, const glm::mat4& scene,
                        const glm::mat4& model, const glm::vec3* overrideColor) const
{
    const glm::mat4 transform = scene * model;
    glUniformMatrix4fv(transformLocation, 1, GL_FALSE, glm::value_ptr(transform));
    glUniformMatrix4fv(modelLocation, 1, GL_FALSE, glm::value_ptr(model));
    for (const auto& mesh : meshes) {
        glUniform3fv(baseColorLocation, 1, glm::value_ptr(overrideColor ? *overrideColor : mesh.baseColor));
        glUniform3fv(specularLocation, 1, glm::value_ptr(mesh.dielectricF0));
        glUniform1f(metallicLocation, mesh.metallic);
        glUniform1f(roughnessLocation, mesh.roughness);
        glBindVertexArray(mesh.vao);
        glDrawElements(GL_TRIANGLES, mesh.indexCount, GL_UNSIGNED_INT, nullptr);
    }
    glBindVertexArray(0);
}

void MeshRenderer::Clear()
{
    if (shaderProgramID) {
        glUseProgram(0);
        glDeleteProgram(shaderProgramID);
        shaderProgramID = 0;
    }
}

} // namespace aegis
