#include "turret_view.h"
#include "gameplay/effects/destruction_effects.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace aegis
{

bool TurretView::Init(const std::filesystem::path& root)
{
    const glm::vec3 corners[] = {{-.5f, -.5f, -.5f}, {.5f, -.5f, -.5f}, {.5f, .5f, -.5f}, {-.5f, .5f, -.5f},
                                 {-.5f, -.5f, .5f},  {.5f, -.5f, .5f},  {.5f, .5f, .5f},  {-.5f, .5f, .5f}};
    const unsigned indices[] = {0, 2, 1, 0, 3, 2, 4, 5, 6, 4, 6, 7, 0, 4, 7, 0, 7, 3,
                                1, 2, 6, 1, 6, 5, 0, 1, 5, 0, 5, 4, 3, 7, 6, 3, 6, 2};
    glm::vec3 vertices[36];
    for (int i = 0; i < 36; ++i)
    {
        vertices[i] = corners[indices[i]];
    }
    return InitPrimitive(root / "shaders/turret.vert", root / "shaders/turret.frag",
                         "baseColor", vertices, sizeof(vertices), 3, sizeof(glm::vec3));
}

void TurretView::Box(const glm::mat4& viewProjection, const glm::mat4& parent, const glm::vec3& low,
                     const glm::vec3& high, const glm::vec3& color) const
{
    const auto transform =
        viewProjection * parent * glm::scale(glm::translate(glm::mat4(1), (low + high) * .5f), high - low);
    glUniformMatrix4fv(transformLocation, 1, GL_FALSE, glm::value_ptr(transform));
    glUniform3fv(colorLocation, 1, glm::value_ptr(color));
    glDrawArrays(GL_TRIANGLES, 0, 36);
}

void TurretView::DrawDebris(const glm::mat4& viewProjection, const glm::mat4& world, float age) const
{
    if (!Ready())
    {
        return;
    }
    Begin();
    for (int i = 0; i < combat::DestructionEffects::TurretPieceCount; ++i)
    {
        const auto parent = combat::DestructionEffects::PieceTransform(world, i, age);
        Box(viewProjection, parent, {-.5f, i * .73f, -.5f}, {.5f, (i + 1) * .73f, .5f}, {.35f, .4f, .46f});
    }
    End();
}
} // namespace aegis
