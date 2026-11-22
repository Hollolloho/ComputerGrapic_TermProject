#include "bullet.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <cmath>
#include <vector>

namespace aegis
{
bool BulletView::Init(const std::filesystem::path& shaderDirectory)
{
    // +Z가 앞이다. 끝부분의 단면을 반 칸씩 돌려 짧은 삼각 절단면을 만든다.
    struct Cut { float z, radius, rotZDegrees; };
    const int sideCount = 12;
    const int cutCount = 5;
    const Cut cuts[cutCount] = {
        {-1.18f, 0.43f, -15.f}, {-0.80f, 0.74f, 0.f}, {0.32f, 0.87f, 0.f},
        {0.85f, 0.72f, 0.f}, {1.91f, 0.32f, 15.f}
    };
    glm::vec3 rings[cutCount][sideCount];
    for (int ring = 0; ring < cutCount; ++ring)
    {
        const Cut& cut = cuts[ring];
        for (int side = 0; side < sideCount; ++side)
        {
            const float angle = glm::radians(360.f * side / sideCount + cut.rotZDegrees);
            rings[ring][side] = {cut.radius * std::cos(angle), cut.radius * std::sin(angle), cut.z};
        }
    }

    // 정점의 w는 면 안의 가로 위치다. 셰이더가 이 값으로 모서리의 반사 폭을 잡는다.
    const glm::vec4 tip(0.05f, 0.02f, 2.62f, 0.f), tail(-0.06f, 0.03f, -1.43f, 0.f);
    std::vector<glm::vec4> vertices;
    for (int side = 0; side < sideCount; ++side)
    {
        const int next = (side + 1) % sideCount;
        for (int ring = 0; ring + 1 < cutCount; ++ring)
        {
            const glm::vec4 backA(rings[ring][side], 0.f), backB(rings[ring][next], 1.f);
            const glm::vec4 frontA(rings[ring + 1][side], 0.f), frontB(rings[ring + 1][next], 1.f);
            vertices.insert(vertices.end(), {frontA, backA, backB, frontA, backB, frontB});
        }
        vertices.insert(vertices.end(), {
            tip, glm::vec4(rings[cutCount - 1][side], 0.f), glm::vec4(rings[cutCount - 1][next], 1.f),
            tail, glm::vec4(rings[0][next], 1.f), glm::vec4(rings[0][side], 0.f)
        });
    }
    vertexCount = static_cast<GLsizei>(vertices.size());
    if (!InitPrimitive(shaderDirectory / "bullet.vert", shaderDirectory / "bullet.frag",
                       "color", vertices.data(), vertexCount * sizeof(glm::vec4), 4, sizeof(glm::vec4)))
        return false;

    modelViewLocation = glGetUniformLocation(program, "modelView");
    if (modelViewLocation < 0)
    {
        Clear();
        return false;
    }
    return true;
}

void BulletView::Draw(const glm::mat4& projection, const glm::mat4& view, const combat::Bullet& bullet,
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

    const glm::mat4 modelView = view * model;
    Begin();
    glUniformMatrix4fv(transformLocation, 1, GL_FALSE, glm::value_ptr(projection * modelView));
    glUniformMatrix4fv(modelViewLocation, 1, GL_FALSE, glm::value_ptr(modelView));
    glUniform3fv(colorLocation, 1, glm::value_ptr(color));
    glDrawArrays(GL_TRIANGLES, 0, vertexCount);
    End();
}
}
