#pragma once

#include "RenderMesh.h"
#include <glm/mat4x4.hpp>

#include <array>
#include <filesystem>
#include <vector>

namespace aegis {

class Drone {
public:
    Drone();
    Drone(const Drone&) = delete;
    Drone& operator=(const Drone&) = delete;

    bool Init(const std::filesystem::path& projectDirectory);
    void Update(double deltaTime);
    void Draw(const glm::mat4& scene, const glm::mat4& world, bool warning=false, bool armed=true, bool staticPose=false,float debrisAge=-1) const;
    void Reset();
    void UpdateCombat(double gameDt);
    void Clear();

private:
    struct DronePart {
        const char* name;
        glm::vec3 origin;
        glm::vec3 rotationAxis;
        std::vector<const char*> materials;
        float angleDegrees{};
        std::vector<RenderMesh> meshes;
    };

    enum PartIndex { Body, Sensor, LeftFan, RightFan, Weapon, PartCount };

    static glm::mat4 PartTransform(const glm::mat4& world, const DronePart& part);

    std::array<DronePart, PartCount> parts;
    double elapsedTime{};
    GLuint shaderProgram{};
    GLint transformLocation{-1},modelLocation{-1},colorLocation{-1},emissionLocation{-1};
};

} // namespace aegis
