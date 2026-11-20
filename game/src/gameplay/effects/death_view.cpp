#include "death_view.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>
#include <cmath>

namespace aegis
{
bool DeathView::Init(const std::filesystem::path& root)
{
    const float vertices[] = {-.5f, -.45f, .5f, -.35f, 0, .6f};
    return InitPrimitive(root / "shaders/death_effect.vert", root / "shaders/death_effect.frag",
                         "color", vertices, sizeof(vertices), 2, 2 * sizeof(float));
}

void DeathView::Draw(float progress, float aspect) const
{
    if (!Ready() || !std::isfinite(progress) || progress >= 1 || aspect <= 0)
    {
        return;
    }
    const float p = std::clamp(progress, 0.0f, 1.0f);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    Begin();
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
    End();
    glDisable(GL_BLEND);
    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);
}
} // namespace aegis
