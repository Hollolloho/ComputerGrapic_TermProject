#pragma once

#include "combat_range.h"
#include "player_camera.h"
#include <GL/glew.h>
#include <filesystem>

struct GLFWwindow;

namespace aegis {

class CombatTestScene {
public:
    bool Init();
    void Reset();
    void SampleInput(GLFWwindow* window, bool active);
    void Update(double realDt, double gameDt);
    void Draw(int width, int height) const;
    void Clear();
    void SetSmokeFrame(int frame);
    bool SmokePassed() const;
    bool SaveFrame(const std::filesystem::path& path, int width, int height) const;
    const combat::CombatRange& Range() const { return range; }

private:
    void DrawBox(const glm::mat4& viewProjection, const glm::vec3& minimum,
        const glm::vec3& maximum, const glm::vec3& color) const;
    combat::CombatRange range;
    PlayerCamera camera;
    GLuint program{};
    GLuint vao{};
    GLuint vbo{};
    GLint transformLocation{-1};
    GLint colorLocation{-1};
    bool mouseHeld{};
    bool coverHeld{};
    bool lookHeld{};
    double lastX{};
    double lastY{};
    float yaw{};
    float pitch{};
};

} // namespace aegis
