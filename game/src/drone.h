#pragma once

#include <GL/glew.h>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include <array>
#include <filesystem>
#include <vector>

namespace aegis {

struct ModelMesh;

class Drone {
public:
    Drone();
    Drone(const Drone&) = delete;
    Drone& operator=(const Drone&) = delete;

    bool Init(const std::filesystem::path& projectDirectory);
    void Update(double deltaTime);
    void Draw(const glm::mat4& scene) const;
    void Reset();
    void Clear();

private:
    struct DrawMesh {
        GLuint vao{};
        GLuint vbo{};
        GLuint ebo{};
        GLsizei indexCount{};
        glm::vec3 color{1.0f};
    };

    struct DronePart {
        const char* name;
        glm::vec3 origin;
        glm::vec3 rotationAxis;
        std::vector<const char*> materials;
        float angleDegrees{};
        std::vector<DrawMesh> meshes;
    };

    enum PartIndex { Body, Sensor, LeftFan, RightFan, Weapon, PartCount };

    static void InitVertexBuffer(DrawMesh& drawMesh, const ModelMesh& mesh);
    static glm::mat4 PartTransform(const glm::mat4& world, const DronePart& part);

    std::array<DronePart, PartCount> parts;
    double elapsedTime{};
    GLuint shaderProgramID{};
    GLint transformLocation{-1};
    GLint modelLocation{-1};
    GLint colorLocation{-1};
};

} // namespace aegis
