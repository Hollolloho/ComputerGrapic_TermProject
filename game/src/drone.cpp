#include "drone.h"

#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>
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
    const auto objDirectory = projectDirectory / "asset" / "drone" / "obj";
    const auto materialPath = objDirectory / "materials.mtl";
    if (!std::ifstream(materialPath)) {
        std::cerr << "Could not read drone materials: " << materialPath << '\n';
        return false;
    }

    // OBJ/MTL 읽기와 캐시 처리 시간을 GPU 버퍼 업로드 호출 시간과 따로 잰다.
    using Clock = std::chrono::steady_clock;
    using Milliseconds = std::chrono::duration<double, std::milli>;
    const auto cacheStart = Clock::now();
    const ObjModelCache cache(projectDirectory / "build" / "cache" / "models", materialPath);
    Clock::duration loadTime = Clock::now() - cacheStart;
    Clock::duration uploadTime{};
    int cacheHits{};
    int assimpImports{};

    for (auto& part : parts) {
        for (const char* materialName : part.materials) {
            const auto path = objDirectory / (std::string("drone_") + part.name +
                                              '_' + materialName + ".obj");
            ModelData model;
            std::string error;
            bool cacheHit{};
            const auto loadStart = Clock::now();
            if (!cache.Load(path, model, error, cacheHit)) {
                std::cerr << error << '\n';
                return false;
            }
            loadTime += Clock::now() - loadStart;
            if (cacheHit) {
                ++cacheHits;
            } else {
                ++assimpImports;
            }

            for (const auto& mesh : model.meshes) {
                const auto& material = model.materials[mesh.materialIndex];
                if (material.name != materialName) {
                    std::cerr << path << ": expected material " << materialName << " from "
                              << materialPath << ", found " << material.name << '\n';
                    return false;
                }
                auto& drawMesh = part.meshes.emplace_back();
                const auto uploadStart = Clock::now();
                drawMesh.Upload(mesh, material);
                uploadTime += Clock::now() - uploadStart;
            }
        }
    }
    std::cout << "Drone models: cache_hits=" << cacheHits
              << " assimp_imports=" << assimpImports
              << " cpu_ms=" << Milliseconds(loadTime).count()
              << " gl_upload_ms=" << Milliseconds(uploadTime).count() << '\n';
    return true;
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

void Drone::UpdateCombat(double gameDt) {
    Update(gameDt);
    parts[Sensor].angleDegrees=0;
    parts[Weapon].angleDegrees=0; // Gameplay world pose aims the complete model.
}

glm::mat4 Drone::PartTransform(const glm::mat4& world, const DronePart& part)
{
    // OBJ의 로컬 원점이 피벗이다. 회전 후 조립 위치로 옮긴다.
    const glm::mat4 position = glm::translate(world, part.origin);
    return glm::rotate(position, glm::radians(part.angleDegrees), part.rotationAxis);
}

void Drone::Draw(const MeshRenderer& renderer, const glm::mat4& scene, const glm::mat4& world, bool warning, bool armed) const
{
    for (std::size_t i=0;i<parts.size();++i) {
        if (i==Weapon && !armed) continue;
        const auto& part=parts[i];
        const glm::vec3 red(1,0.04f,0.02f);
        renderer.Draw(part.meshes, scene, PartTransform(world, part), i==Sensor && warning ? &red : nullptr);
    }
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
        for (auto& mesh : part.meshes) {
            mesh.Clear();
        }
        part.meshes.clear();
    }
    Reset();
}

} // namespace aegis
