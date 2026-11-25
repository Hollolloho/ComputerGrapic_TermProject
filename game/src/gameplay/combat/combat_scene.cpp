// 전투 장면의 초기화, 갱신과 종료를 연결한다.
#include "combat_scene.h"
#include "loader/Shader.h"
#include "combat_map/terminal_layout.generated.h"
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>

namespace aegis
{
bool CombatScene::Init(const std::filesystem::path& projectDirectory)
{
    const auto shaderDirectory = projectDirectory / "shaders";
    program = LoadShaderProgram(shaderDirectory / "combat_box.vert", shaderDirectory / "combat_box.frag");
    if (!program)
    {
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
    if (!signs.Init(shaderDirectory) || !deathView.Init(projectDirectory) || !turretView.Init(projectDirectory) ||
        !weapon.Init(projectDirectory) || !bulletView.Init(projectDirectory) || !terminalRenderer.Init(shaderDirectory) ||
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
        if (!gpu.Upload(mesh, model.materials.at(mesh.materialIndex)))
        {
            return false;
        }
    }
    return triangles == terminal_asset::triangles;
}

void CombatScene::Reset()
{
    analysis.Reset();
    ClearPlayerView();
    feedback.Reset();
    destruction.Reset(1 + encounter.ExtraDrones().size() + encounter.Turrets().size());
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
    if (IsBossMode() || IsCoreMode())
    {
        const auto target = IsCoreMode() ? core.CoreCenter() : boss.CoreCenter();
        const auto d = glm::normalize(target - (CameraRoot() + glm::vec3(0, player::EyeHeight, 0)));
        rotX = glm::degrees(std::asin(d.y));
    }
    camera.SetPose(CameraRoot(), rotY, rotX);
    // Held input remains latched until release (R must not create a fresh shot).
    lookHeld = false;
}

void CombatScene::Update(double realDeltaTime, double gameDeltaTime, float strafeAmount)
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
    UpdateVisualEffects(realDeltaTime, gameDeltaTime, strafeAmount);
}

int CombatScene::PlayerHealth() const
{
    return activeBattle->Health();
}

void CombatScene::UpdateBattle(double realDeltaTime, double gameDeltaTime)
{
    const float realTime = static_cast<float>(realDeltaTime);
    const float gameTime = static_cast<float>(gameDeltaTime);
    activeBattle->Update(realTime, gameTime, PlayerEye(), PlayerForward());
}

void CombatScene::UpdateVisualEffects(double realDeltaTime, double gameDeltaTime, float strafeAmount)
{
    destruction.UpdateWorld(static_cast<float>(gameDeltaTime));
    ObserveDestruction();
    destruction.ObserveHealth(PlayerHealth());
    if (BattleResult() == combat::EncounterResult::Running &&
        (IsCoreMode() || (IsDroneMode() && encounter.EnemyAlive()) || !encounter.ExtraDrones().empty()))
    {
        drone.UpdateRotor(gameDeltaTime);
    }
    weapon.Update(realDeltaTime, strafeAmount);
    feedback.Update(static_cast<float>(gameDeltaTime));
    const auto forward = PlayerForward();
    const auto right = glm::normalize(glm::cross(forward, CurrentPlayerView().up));
    const auto origin = PlayerEye() + forward * 0.8f + right * 0.22f + glm::vec3(0, -0.14f, 0);
    feedback.Observe(PlayerHealth(), Inventory().Breaks(), origin, right, forward);
}

void CombatScene::ObserveDestruction()
{
    if (IsCoreMode()) return;
    const auto& state = encounter.DroneState();
    destruction.ObserveEnemy(0, IsDroneMode() && encounter.EnemyAlive(), state.WorldTransform());
    if (IsBossMode()) return;

    // 기본 드론, 추가 드론, 포탑 순서로 파괴 효과 자리를 공유한다.
    std::size_t slot = 1;
    for (const auto& extra : encounter.ExtraDrones())
    {
        destruction.ObserveEnemy(slot++, extra.Phase() != combat::DronePhase::Destroyed, extra.WorldTransform());
    }
    for (const auto& turret : encounter.Turrets())
    {
        destruction.ObserveEnemy(slot++, turret.Alive(), glm::translate(glm::mat4(1), turret.Position()));
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
    ClearDrawMeshes(terminalMeshes);
    signs.Clear();
    deathView.Clear();
    turretView.Clear();
    drone.Clear();
    terminalRenderer.Clear();
    weapon.Clear();
    bulletView.Clear();
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
