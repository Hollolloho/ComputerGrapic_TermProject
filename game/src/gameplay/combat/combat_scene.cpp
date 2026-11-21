// 전투 장면의 초기화, 갱신과 종료를 연결한다.
#include "combat_scene.h"
#include "combat_map/terminal_layout.generated.h"
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>

namespace aegis
{
namespace
{
GLuint Compile(GLenum kind, const char* source)
{
    const GLuint shader = glCreateShader(kind);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    GLint ok{};
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok)
    {
        char log[2048]{};
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        std::cerr << "Combat shader: " << log << '\n';
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}
} // namespace

bool CombatScene::Init(const std::filesystem::path& projectDirectory)
{
    const char* vertex = R"(#version 330 core
layout(location=0) in vec3 position;
uniform mat4 transform;
out vec3 localPosition;
void main() { localPosition=position; gl_Position=transform*vec4(position,1); }
)";
    const char* fragment = R"(#version 330 core
in vec3 localPosition;
uniform vec3 color;
uniform float opacity;
out vec4 outColor;
void main() {
    vec3 normal=normalize(cross(dFdx(localPosition),dFdy(localPosition)));
    float light=0.65+0.35*abs(dot(normal,normalize(vec3(0.4,0.8,0.5))));
    outColor=vec4(color*light,opacity);
})";
    const GLuint vs = Compile(GL_VERTEX_SHADER, vertex);
    const GLuint fs = Compile(GL_FRAGMENT_SHADER, fragment);
    if (!vs || !fs)
    {
        if (vs)
        {
            glDeleteShader(vs);
        }
        if (fs)
        {
            glDeleteShader(fs);
        }
        return false;
    }
    program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);
    glDeleteShader(vs);
    glDeleteShader(fs);
    GLint linked{};
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (!linked)
    {
        Clear();
        return false;
    }
    transformLocation = glGetUniformLocation(program, "transform");
    colorLocation = glGetUniformLocation(program, "color");
    opacityLocation = glGetUniformLocation(program, "opacity");
    const glm::vec3 corners[] = {{-0.5f, -0.5f, -0.5f}, {0.5f, -0.5f, -0.5f}, {0.5f, 0.5f, -0.5f},
                                 {-0.5f, 0.5f, -0.5f},  {-0.5f, -0.5f, 0.5f}, {0.5f, -0.5f, 0.5f},
                                 {0.5f, 0.5f, 0.5f},    {-0.5f, 0.5f, 0.5f}};
    const unsigned indices[] = {0, 2, 1, 0, 3, 2, 4, 5, 6, 4, 6, 7, 0, 4, 7, 0, 7, 3,
                                1, 2, 6, 1, 6, 5, 0, 1, 5, 0, 5, 4, 3, 7, 6, 3, 6, 2};
    std::vector<glm::vec3> vertices;
    for (unsigned index : indices)
    {
        vertices.push_back(corners[index]);
    }
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(glm::vec3), vertices.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), nullptr);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);
    Reset();
    if (!signs.Init() || !deathView.Init(projectDirectory) || !turretView.Init(projectDirectory) ||
        !weapon.Init(projectDirectory) || !terminalRenderer.Init(projectDirectory / "shaders") ||
        !drone.Init(projectDirectory) || !InitTerminal(projectDirectory) || !bossView.Init(projectDirectory) ||
        !coreView.Init(projectDirectory) || !singleModel.Init(projectDirectory, combat::TurretType::SingleC) ||
        !twinModel.Init(projectDirectory, combat::TurretType::Twin))
    {
        Clear();
        return false;
    }
    return true;
}

bool CombatScene::InitTerminal(const std::filesystem::path& root)
{
    ModelData model;
    std::string error;
    if (!LoadModel(root / terminal_asset::mesh, model, error))
    {
        std::cerr << error << '\n';
        return false;
    }
    std::size_t triangles = 0;
    for (const auto& mesh : model.meshes)
    {
        triangles += mesh.indices.size() / 3;
        auto& gpu = terminalMeshes.emplace_back();
        gpu.Upload(mesh, model.materials.at(mesh.materialIndex));
    }
    return triangles == terminal_asset::triangles;
}

void CombatScene::Reset()
{
    analysis.Reset();
    ClearPlayerView();
    feedback.Reset();
    destruction.Reset();
    time.Reset();
    forceNormal = false;
    movementTimeAvailable = false;
    movementScale = 0.05;
    encounter.Reset();
    weapon.Reset();
    drone.Reset();
    boss.Reset();
    core.Reset();
    bossCovered = false;
    rotY = rotX = 0;
    if (bossMode || coreMode)
    {
        const auto target = coreMode ? core.CoreCenter() : boss.CoreCenter();
        const auto d = glm::normalize(target - (CameraRoot() + glm::vec3(0, 1.65f, 0)));
        rotX = glm::degrees(std::asin(d.y));
    }
    camera.SetPose(CameraRoot(), rotY, rotX);
    // Held input remains latched until release (R must not create a fresh shot).
    lookHeld = false;
}

void CombatScene::Update(double realDeltaTime, double gameDeltaTime)
{
    if (analysis.Active())
    {
        return;
    }
    if (DeathStarted())
    {
        destruction.UpdateDeath(static_cast<float>(realDeltaTime));
        return;
    }
    ObserveDestruction();
    const auto frame = time.Advance(realDeltaTime, gameDeltaTime, forceNormal);
    realDeltaTime = frame.realDeltaTime;
    gameDeltaTime = frame.gameDeltaTime;
    if (realDeltaTime <= 0)
    {
        return;
    }
    // 충돌·피해를 먼저 처리하고, 확정된 결과로 애니메이션과 파편을 갱신한다.
    UpdateBattle(realDeltaTime, gameDeltaTime);
    UpdateVisualEffects(realDeltaTime, gameDeltaTime);
}

int CombatScene::PlayerHealth() const
{
    return ActiveEncounter()->Health();
}

void CombatScene::UpdateBattle(double realDeltaTime, double gameDeltaTime)
{
    const float realTime = static_cast<float>(realDeltaTime);
    const float gameTime = static_cast<float>(gameDeltaTime);
    ActiveEncounter()->Update(realTime, gameTime, PlayerEye(), PlayerForward());
}

void CombatScene::UpdateVisualEffects(double realDeltaTime, double gameDeltaTime)
{
    destruction.UpdateWorld(static_cast<float>(gameDeltaTime));
    ObserveDestruction();
    destruction.ObserveHealth(PlayerHealth());
    if ((coreMode && core.Result() == combat::EncounterResult::Running) ||
        (IsDroneMode() && encounter.EnemyAlive() && encounter.Result() == combat::EncounterResult::Running))
    {
        drone.UpdateRotor(gameDeltaTime);
    }
    weapon.Update(realDeltaTime);
    feedback.Update(static_cast<float>(gameDeltaTime));
    const auto forward = PlayerForward();
    const auto right = glm::normalize(glm::cross(forward, CurrentPlayerView().up));
    const auto origin = PlayerEye() + forward * 0.8f + right * 0.22f + glm::vec3(0, -0.14f, 0);
    feedback.Observe(PlayerHealth(), Inventory().Breaks(), origin, right, forward);
}

void CombatScene::ObserveDestruction()
{
    if (coreMode) return;
    const auto& state = encounter.DroneState();
    destruction.ObserveEnemy(0, IsDroneMode() && encounter.EnemyAlive(), state.WorldTransform());
    for (std::size_t i = 0; !bossMode && i < encounter.Turrets().size(); ++i)
    {
        const auto& turret = encounter.Turrets()[i];
        destruction.ObserveEnemy(static_cast<int>(i + 1), turret.Alive(),
                                 glm::translate(glm::mat4(1), turret.Position()));
    }
}

void CombatScene::Clear()
{
    analysis.Reset();
    ClearPlayerView();
    feedback.Reset();
    bossView.Clear();
    coreView.Clear();
    singleModel.Clear();
    twinModel.Clear();
    for (auto& mesh : terminalMeshes)
    {
        mesh.Clear();
    }
    terminalMeshes.clear();
    signs.Clear();
    deathView.Clear();
    turretView.Clear();
    drone.Clear();
    terminalRenderer.Clear();
    weapon.Clear();
    if (vbo)
    {
        glDeleteBuffers(1, &vbo);
    }
    if (vao)
    {
        glDeleteVertexArrays(1, &vao);
    }
    if (program)
    {
        glDeleteProgram(program);
    }
    vbo = vao = program = 0;
}

} // namespace aegis
