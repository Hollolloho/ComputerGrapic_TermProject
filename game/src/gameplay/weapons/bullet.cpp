#include "bullet.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <cmath>
#include <iterator>

namespace aegis
{
namespace
{
// +Z가 탄의 앞쪽이다. 가운데 네 꼭짓점을 앞뒤 끝과 연결한다.
const glm::vec3 tip(0, 0, 2.5f), tail(0, 0, -1.3f);
const glm::vec3 right(1, 0, 0), top(0, 1, 0), left(-1, 0, 0), bottom(0, -1, 0);
const glm::vec3 vertices[] = {
    tip, right, top,     tip, top, left,     tip, left, bottom,     tip, bottom, right,
    tail, top, right,    tail, left, top,    tail, bottom, left,    tail, right, bottom
};
}

bool BulletView::Init(const std::filesystem::path& shaderDirectory)
{
    return InitPrimitive(shaderDirectory / "bullet.vert", shaderDirectory / "bullet.frag",
                         "color", vertices, sizeof(vertices), 3, sizeof(glm::vec3));
}

void BulletView::Draw(const glm::mat4& scene, const combat::Bullet& bullet,
                      const glm::vec3& color, float radius) const
{
    if (!Ready() || !bullet.active)
        return;

    const glm::vec3& v = bullet.velocity;
    const float rotY = std::atan2(v.x, v.z);
    const float rotX = -std::atan2(v.y, glm::length(glm::vec2(v.x, v.z)));
    glm::mat4 model = glm::translate(glm::mat4(1), bullet.position);
    model = glm::rotate(model, rotY, glm::vec3(0, 1, 0));
    model = glm::rotate(model, rotX, glm::vec3(1, 0, 0));
    model = glm::scale(model, glm::vec3(radius));

    Begin();
    glUniformMatrix4fv(transformLocation, 1, GL_FALSE, glm::value_ptr(scene * model));
    glUniform3fv(colorLocation, 1, glm::value_ptr(color));
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(std::size(vertices)));
    End();
}
}
