#include "drone.h"
#include "model_loader.h"

#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <cmath>
#include <cstddef>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

namespace aegis {
namespace {

constexpr double SensorYawDegrees = 35.0;
constexpr double SensorPeriod = 4.0;
constexpr double FanDegreesPerSecond = 240.0;
constexpr double FullTurnDegrees = 360.0;
constexpr double WeaponPitchDegrees = 12.0;
constexpr double WeaponPeriod = 3.0;

const glm::vec3 YawAxis(0.0f, 1.0f, 0.0f);
const glm::vec3 PitchAxis(1.0f, 0.0f, 0.0f);

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

Drone::Drone()
    // 조립 원점과 회전축은 assembly.json의 값을 사용한다. OBJ 좌표 단위는 미터다.
    : parts{{
        {"body",        { 0.00f,  0.000f,  0.00f}, YawAxis,   {"shell", "white", "dark", "steel", "red"}},
        {"sensor",      { 0.00f, -0.015f,  0.49f}, YawAxis,   {"rubber", "steel", "red", "lens"}},
        {"rotor_left",  {-1.02f,  0.040f, -0.02f}, YawAxis,   {"dark", "rubber", "steel"}},
        {"rotor_right", { 1.02f,  0.040f, -0.02f}, YawAxis,   {"dark", "rubber", "steel"}},
        {"weapon",      { 0.00f, -0.350f,  0.13f}, PitchAxis, {"shell", "dark", "rubber", "steel", "red"}},
    }}
{
}

bool Drone::Init(const std::filesystem::path& projectDirectory)
{
    Clear();
    const auto shaderDirectory = projectDirectory / "game" / "shaders";
    shaderProgramID = MakeShaderProgram(shaderDirectory);
    if (!shaderProgramID) {
        return false;
    }

    transformLocation = glGetUniformLocation(shaderProgramID, "uTransform");
    modelLocation = glGetUniformLocation(shaderProgramID, "uModel");
    colorLocation = glGetUniformLocation(shaderProgramID, "uColor");
    if (transformLocation == -1 || modelLocation == -1 || colorLocation == -1) {
        std::cerr << "Drone shader uniforms uTransform, uModel or uColor are missing: "
                  << shaderDirectory << '\n';
        return false;
    }

    const auto objDirectory = projectDirectory / "asset" / "drone" / "obj";
    const auto materialPath = objDirectory / "materials.mtl";
    if (!std::ifstream(materialPath)) {
        std::cerr << "Could not read drone materials: " << materialPath << '\n';
        return false;
    }

    for (auto& part : parts) {
        for (const char* materialName : part.materials) {
            const auto path = objDirectory / (std::string("drone_") + part.name +
                                              '_' + materialName + ".obj");
            ModelData model;
            std::string error;
            if (!LoadModel(path, model, error)) {
                std::cerr << error << '\n';
                return false;
            }

            for (const auto& mesh : model.meshes) {
                const auto& material = model.materials[mesh.materialIndex];
                if (material.name != materialName) {
                    std::cerr << path << ": expected material " << materialName << " from "
                              << materialPath << ", found " << material.name << '\n';
                    return false;
                }
                auto& drawMesh = part.meshes.emplace_back();
                drawMesh.color = material.diffuse;
                InitVertexBuffer(drawMesh, mesh);
            }
        }
    }
    return true;
}

void Drone::InitVertexBuffer(DrawMesh& drawMesh, const ModelMesh& mesh)
{
    // VAO에 정점 속성과 인덱스 버퍼 연결을 기록한다.
    glGenVertexArrays(1, &drawMesh.vao);
    glGenBuffers(1, &drawMesh.vbo);
    glGenBuffers(1, &drawMesh.ebo);
    glBindVertexArray(drawMesh.vao);

    glBindBuffer(GL_ARRAY_BUFFER, drawMesh.vbo);
    glBufferData(GL_ARRAY_BUFFER, mesh.vertices.size() * sizeof(ModelVertex),
                 mesh.vertices.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, drawMesh.ebo);
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
    drawMesh.indexCount = static_cast<GLsizei>(mesh.indices.size());
}

void Drone::Update(double deltaTime)
{
    elapsedTime += deltaTime;

    // 센서와 발사기는 정면에서 시작해 양쪽으로 왕복한다.
    const double sensorPhase = glm::two_pi<double>() * elapsedTime / SensorPeriod;
    const double weaponPhase = glm::two_pi<double>() * elapsedTime / WeaponPeriod;
    parts[Sensor].angleDegrees = static_cast<float>(SensorYawDegrees * std::sin(sensorPhase));
    parts[Weapon].angleDegrees = static_cast<float>(WeaponPitchDegrees * std::sin(weaponPhase));

    // 두 팬은 같은 속도로 반대 방향으로 돌린다.
    const double fanAngle = std::fmod(FanDegreesPerSecond * elapsedTime, FullTurnDegrees);
    parts[LeftFan].angleDegrees = static_cast<float>(fanAngle);
    parts[RightFan].angleDegrees = -parts[LeftFan].angleDegrees;
}

glm::mat4 Drone::PartTransform(const glm::mat4& world, const DronePart& part)
{
    // OBJ의 로컬 원점이 피벗이다. 회전 후 조립 위치로 옮긴다.
    const glm::mat4 position = glm::translate(world, part.origin);
    return glm::rotate(position, glm::radians(part.angleDegrees), part.rotationAxis);
}

void Drone::Draw(const glm::mat4& scene) const
{
    glUseProgram(shaderProgramID);
    const glm::mat4 world(1.0f);
    for (const auto& part : parts) {
        const glm::mat4 model = PartTransform(world, part);
        const glm::mat4 transform = scene * model;
        glUniformMatrix4fv(transformLocation, 1, GL_FALSE, glm::value_ptr(transform));
        glUniformMatrix4fv(modelLocation, 1, GL_FALSE, glm::value_ptr(model));
        for (const auto& mesh : part.meshes) {
            glUniform3fv(colorLocation, 1, glm::value_ptr(mesh.color));
            glBindVertexArray(mesh.vao);
            glDrawElements(GL_TRIANGLES, mesh.indexCount, GL_UNSIGNED_INT, nullptr);
        }
    }
    glBindVertexArray(0);
}

void Drone::Reset()
{
    elapsedTime = 0.0;
    for (auto& part : parts) {
        part.angleDegrees = 0.0f;
    }
}

void Drone::Clear()
{
    for (auto& part : parts) {
        for (const auto& mesh : part.meshes) {
            glDeleteVertexArrays(1, &mesh.vao);
            glDeleteBuffers(1, &mesh.vbo);
            glDeleteBuffers(1, &mesh.ebo);
        }
        part.meshes.clear();
    }
    if (shaderProgramID) {
        glUseProgram(0);
        glDeleteProgram(shaderProgramID);
        shaderProgramID = 0;
    }
    transformLocation = -1;
    modelLocation = -1;
    colorLocation = -1;
    Reset();
}

} // namespace aegis
