#pragma once
#include <filesystem>
#include <GL/glew.h>
namespace aegis {
class DeathView {
public:
    bool Init(const std::filesystem::path& root);
    void Draw(float progress,float aspect) const;
    void Clear();
private:
    GLuint program{},vao{},vbo{};
    GLint transformLocation{-1},colorLocation{-1};
};
}
