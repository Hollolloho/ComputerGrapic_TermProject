#include "death_view.h"
#include "Shader.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>
#include <cmath>

namespace aegis
{
bool DeathView::Init(const std::filesystem::path& root)
{
    Clear();
    program =
        LoadShaderProgram(root / "game/shaders/death_effect.vert", root / "game/shaders/death_effect.frag");
    if (!program)
    {
        return false;
    }
    transformLocation = glGetUniformLocation(program, "transform");
    colorLocation = glGetUniformLocation(program, "color");
    if (transformLocation < 0 || colorLocation < 0)
    {
        Clear();
        return false;
    }
    const float vertices[] = {-.5f, -.45f, .5f, -.35f, 0, .6f};
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    return true;
}

void DeathView::Draw(float progress, float aspect) const
{
    if (!program || !std::isfinite(progress) || progress >= 1 || aspect <= 0)
    {
        return;
    }
    const float p = std::clamp(progress, 0.0f, 1.0f);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glUseProgram(program);
    glBindVertexArray(vao);
    for (int i = 0; i < 24; ++i)
    {
        const float angle = i * 2.39996323f, radius = .12f + (i % 6) * .16f + p * (.5f + (i % 4) * .22f);
        const auto transform = glm::scale(
            glm::rotate(glm::translate(glm::mat4(1), glm::vec3(std::cos(angle) * radius / aspect,
                                                               std::sin(angle) * radius - p * p * .35f, 0)),
                        angle + p * (i % 3 + 1), glm::vec3(0, 0, 1)),
            glm::vec3((.17f + .025f * (i % 4)) / aspect, .3f + .03f * (i % 5), 1));
        glUniformMatrix4fv(transformLocation, 1, GL_FALSE, glm::value_ptr(transform));
        glUniform4f(colorLocation, i % 3 ? .85f : .25f, .035f, .06f, (1 - p) * .85f);
        glDrawArrays(GL_TRIANGLES, 0, 3);
    }
    glBindVertexArray(0);
    glUseProgram(0);
    glDisable(GL_BLEND);
    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);
}

void DeathView::Clear()
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
