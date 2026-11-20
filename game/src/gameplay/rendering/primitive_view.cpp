#include "primitive_view.h"
#include "loader/Shader.h"

namespace aegis
{

bool PrimitiveView::InitPrimitive(const std::filesystem::path& vertex, const std::filesystem::path& fragment,
                                 const char* colorUniform, const void* vertices, GLsizeiptr byteSize,
                                 GLint components, GLsizei stride)
{
    Clear();
    program = LoadShaderProgram(vertex, fragment);
    if (!program)
    {
        return false;
    }
    transformLocation = glGetUniformLocation(program, "transform");
    colorLocation = glGetUniformLocation(program, colorUniform);
    if (transformLocation < 0 || colorLocation < 0)
    {
        Clear();
        return false;
    }
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, byteSize, vertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, components, GL_FLOAT, GL_FALSE, stride, nullptr);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    return true;
}

void PrimitiveView::Begin() const
{
    glUseProgram(program);
    glBindVertexArray(vao);
}

void PrimitiveView::End() const
{
    glBindVertexArray(0);
    glUseProgram(0);
}

void PrimitiveView::Clear()
{
    if (program)
    {
        glDeleteProgram(program);
    }
    glDeleteBuffers(1, &vbo);
    glDeleteVertexArrays(1, &vao);
    program = vao = vbo = 0;
}

} // namespace aegis
