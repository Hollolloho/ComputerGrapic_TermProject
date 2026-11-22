#pragma once

#include <GL/glew.h>
#include <filesystem>

namespace aegis
{

class PrimitiveView
{
protected:
    bool InitPrimitive(const std::filesystem::path& vertex, const std::filesystem::path& fragment,
                       const char* colorUniform, const void* vertices, GLsizeiptr byteSize,
                       GLint components, GLsizei stride);
    bool Ready() const { return program != 0; }
    void Begin() const;
    void End() const;
    void Clear();

    GLint transformLocation{-1};
    GLint colorLocation{-1};
    GLuint program{};

private:
    GLuint vao{};
    GLuint vbo{};
};

} // namespace aegis
