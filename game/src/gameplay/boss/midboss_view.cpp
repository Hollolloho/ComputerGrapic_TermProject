#include "midboss_view.h"
#include <glm/gtc/type_ptr.hpp>

namespace aegis
{
bool MidbossView::Init(const std::filesystem::path& root)
{
    const glm::vec3 corners[] = {{-1,-1,0}, {1,-1,0}, {1,1,0}, {-1,1,0},
                                 {-1,-1,1}, {1,-1,1}, {1,1,1}, {-1,1,1}};
    const unsigned indices[] = {0,2,1, 0,3,2, 4,5,6, 4,6,7, 0,4,7, 0,7,3,
                                1,2,6, 1,6,5, 0,1,5, 0,5,4, 3,7,6, 3,6,2};
    glm::vec3 vertices[36];
    for (int i = 0; i < 36; ++i)
        vertices[i] = corners[indices[i]];
    if (!InitPrimitive(root / "shaders/midboss_laser.vert", root / "shaders/midboss_laser.frag",
                       "beamColor", vertices, sizeof(vertices), 3, sizeof(glm::vec3)))
        return false;
    eyeLocation = glGetUniformLocation(program, "beamEye");
    sizeLocation = glGetUniformLocation(program, "beamSize");
    strengthLocation = glGetUniformLocation(program, "strength");
    return eyeLocation >= 0 && sizeLocation >= 0 && strengthLocation >= 0;
}

void MidbossView::DrawBeam(const glm::mat4& scene, const glm::vec3& eye, const combat::Midboss& boss) const
{
    const auto phase = boss.Phase();
    const auto& beam = boss.Beam();
    if ((phase != combat::LaserPhase::Charging && phase != combat::LaserPhase::Firing) || beam.length <= .001f)
        return;

    const bool firing = phase == combat::LaserPhase::Firing;
    const float charge = boss.ChargeProgress();
    const float width = firing ? beam.halfWidth : glm::mix(.018f, .047f, charge);
    const float strength = firing ? 1.f : glm::mix(.38f, .82f, charge);
    const glm::vec3 size(width, width, beam.length);
    const auto offset = eye - beam.origin;
    const glm::vec3 localEye(glm::dot(offset, beam.right) / width, glm::dot(offset, beam.up) / width,
                            glm::dot(offset, beam.direction) / beam.length);
    const glm::mat4 model(glm::vec4(beam.right * width, 0), glm::vec4(beam.up * width, 0),
                          glm::vec4(beam.direction * beam.length, 0), glm::vec4(beam.origin, 1));
    const auto transform = scene * model;

    glDepthMask(GL_FALSE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    Begin();
    glUniformMatrix4fv(transformLocation, 1, GL_FALSE, glm::value_ptr(transform));
    glUniform3f(colorLocation, 1.f, .29f, .055f);
    glUniform3fv(eyeLocation, 1, glm::value_ptr(localEye));
    glUniform3fv(sizeLocation, 1, glm::value_ptr(size));
    glUniform1f(strengthLocation, strength);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    End();
    glDisable(GL_BLEND);
    glDepthMask(GL_TRUE);
}
} // namespace aegis
