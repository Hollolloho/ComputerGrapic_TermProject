#pragma once

#include <GL/glew.h>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <array>
#include <filesystem>
#include <vector>

namespace aegis {

// Camera-local viewmodel only; gameplay projectile origin stays in the camera.
class WeaponView {
public:
    WeaponView() = default;
    WeaponView(const WeaponView&) = delete;
    WeaponView& operator=(const WeaponView&) = delete;
    bool Init(const std::filesystem::path& projectDirectory);
    void Reset();
    void OnShot();
    void Update(double realDt);
    void Draw(float aspect) const;
    void Clear();
    int ShotCount() const { return shotCount; }

private:
    struct Mesh {
        GLuint vao{},vbo{},ebo{};
        GLsizei indices{};
        glm::vec3 offset{0};
    };
    std::vector<Mesh> meshes;
    std::array<GLuint,5> textures{};
    GLuint program{};
    GLint transformLocation{-1},modelLocation{-1},shotLocation{-1};
    float recoil{};
    int shotCount{};
};

} // namespace aegis
