// 전투 장면의 초기화, 구역 설정, 갱신과 종료를 연결한다.
#include "combat_scene.h"
#include "corridor_layout.generated.h"
#include "terminal_layout.generated.h"

#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>
#include <fstream>
#include <iostream>
#include <vector>
#include <array>
#include <limits>

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
        std::cerr << "Combat range shader: " << log << '\n';
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
        !drone.Init(projectDirectory) || !InitTerminal(projectDirectory) || !bossView.Init(projectDirectory))
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

bool CombatScene::SetPlayerView(const PlayerView& view)
{
    if (analysis.Active() || DeathStarted())
    {
        return false;
    }
    if (!externalView.Set(view))
    {
        return false;
    }
    lookHeld = false;
    return true;
}

PlayerView CombatScene::CurrentPlayerView() const
{
    const PlayerView fallback{camera.EyePosition(), camera.ViewDirection(), {0, 1, 0}, 60};
    return externalView.Selected(fallback);
}

void CombatScene::Reset()
{
    analysis.Reset();
    ClearPlayerView();
    feedback.Reset();
    destruction.Reset();
    time.Reset();
    slowPreview = false;
    forceNormal = false;
    movementTimeAvailable = manualNormalTime = false;
    movementScale = 0.05;
    range.Reset();
    encounter.Reset();
    weapon.Reset();
    drone.Reset();
    boss.Reset();
    bossCovered = false;
    yaw = pitch = 0;
    if (bossMode)
    {
        const auto d = glm::normalize(boss.CoreCenter() - (CameraRoot() + glm::vec3(0, 1.65f, 0)));
        pitch = glm::degrees(std::asin(d.y));
    }
    camera.SetPose(CameraRoot(), yaw, pitch);
    // Held input remains latched until release (R must not create a fresh shot).
    lookHeld = false;
}

void CombatScene::SetCorridorMode(bool enabled)
{
    bossMode = false;
    combatMapMode = false;
    maintenanceMode = false;
    encounter.SetDroneScale(1);
    encounter.SetSupportTurret(false);
    encounter.SetDroneSpawn({-0.65f, 1.8f, -10}, 0, 0);
    encounter.SetSupply(combat::WeaponInventory::SupplyPosition());
    corridorMode = enabled;
    std::vector<combat::Collider> walls;
    if (enabled)
    {
        walls.assign(corridor_asset::walls.begin(), corridor_asset::walls.end());
        walls.push_back(terminal_asset::collider);
    }
    encounter.SetEnvironment(walls);
    encounter.SetExitDoors(enabled ? 10010 : 0, enabled ? 10011 : 0,
                           enabled ? corridor_asset::doorTravel : 0);
    encounter.SetDroneMode(enabled);
    SetTurretMode(enabled);
}

void CombatScene::SetCombatMapMode(bool enabled)
{
    if (!enabled)
    {
        SetCorridorMode(true);
        return;
    }
    maintenanceMode = false;
    encounter.SetDroneScale(1);
    combatMapMode = true;
    corridorMode = turretMode = true;
    bossMode = false;
    std::vector<combat::Collider> walls(combat_map_asset::walls.begin(), combat_map_asset::walls.end());
    walls.push_back(TerminalCollider());
    encounter.SetEnvironment(walls);
    encounter.SetExitDoors(0, 0, 0);
    encounter.SetDroneSpawn(combat_map_asset::droneSpawn, glm::radians(combat_map_asset::droneYawDegrees),
                            combat_map_asset::droneSpawn.x);
    encounter.SetSupply(combat_map_asset::supply);
    encounter.SetDroneMode(true);
    encounter.SetSupportTurret(true);
    Reset();
    smokeMapSlow = smokeMapCoverBlocked = smokeFailureSeen = smokeMapDeath = smokeMapBoth =
        smokeMapTurretShot = smokeMapLocked = false;
}

// 친구가 배치한 두 번째 드론으로 정비 복도 전투를 구성한다.
void CombatScene::SetMaintenanceMode()
{
    SetCombatMapMode(true);
    maintenanceMode = true;
    encounter.SetSupportTurret(false);

    std::vector<combat::Collider> walls(combat_map_asset::maintenanceWalls.begin(),
                                        combat_map_asset::maintenanceWalls.end());
    walls.push_back(TerminalCollider());
    encounter.SetEnvironment(walls);

    const auto& placement = combat_map_asset::friendDrones[1];
    encounter.SetDroneSpawn(placement.position, glm::radians(placement.yawDegrees), placement.position.x);
    encounter.SetDroneScale(placement.scale);
    encounter.SetSupply(SupplyPosition());
    Reset();
}

void CombatScene::SetDroneMode(bool enabled)
{
    encounter.SetDroneMode(enabled);
    Reset();
    smokeFailureSeen = smokePatrolSeen = smokeWarningSeen = smokeShotSeen = smokeSecuringSeen =
        smokeAnalysisSeen = smokeAnalysisReturned = smokeWorldFrozen = false;
}

void CombatScene::SetTurretMode(bool enabled)
{
    turretMode = enabled;
    Reset();
    smokeFailureSeen = false;
}

glm::vec3 CombatScene::CameraRoot() const
{
    if (bossMode)
    {
        if (bossCovered)
        {
            return combat::BossEncounter::CoverRoot();
        }
        return combat::BossEncounter::FiringRoot();
    }
    if (maintenanceMode)
    {
        return combat_map_asset::maintenanceStart;
    }
    if (combatMapMode)
    {
        return combat_map_asset::playerStart;
    }
    return glm::vec3(0);
}

void CombatScene::SetBossMode(bool enabled)
{
    bossMode = enabled;
    if (enabled)
    {
        combatMapMode = false;
        maintenanceMode = false;
    }
    Reset();
    smokeBossFailureSeen = false;
}

void CombatScene::ToggleBossCover()
{
    bossCovered = !bossCovered;
    const auto target = bossCovered ? boss_asset::supplyInteraction : boss.CoreCenter();
    const auto d = glm::normalize(target - (CameraRoot() + glm::vec3(0, 1.65f, 0)));
    yaw = glm::degrees(std::atan2(-d.x, -d.z));
    pitch = glm::degrees(std::asin(d.y));
    camera.SetPose(CameraRoot(), yaw, pitch);
    lookHeld = false;
}

bool CombatScene::Fire(const glm::vec3& direction)
{
    if (!time.CanFire())
    {
        return false;
    }
    bool fired = false;
    if (bossMode)
    {
        fired = boss.Fire(PlayerEye(), direction);
    }
    else if (turretMode)
    {
        fired = encounter.Fire(PlayerEye(), direction);
    }
    else
    {
        fired = range.Fire(PlayerEye(), direction);
    }
    if (fired)
    {
        time.RequestShot();
        weapon.OnShot();
    }
    return fired;
}

const combat::WeaponInventory& CombatScene::Inventory() const
{
    if (bossMode)
    {
        return boss.Inventory();
    }
    if (turretMode)
    {
        return encounter.Inventory();
    }
    return range.Inventory();
}

bool CombatScene::Interact(const glm::vec3& direction)
{
    if (bossMode)
    {
        return boss.Interact(PlayerEye(), direction);
    }
    if (TerminalTarget(direction))
    {
        return analysis.Open(CanAnalyze(), TerminalCollider(), PlayerEye(), direction, encounter.Colliders());
    }
    return turretMode ? encounter.Interact(PlayerEye(), direction) : range.Interact(PlayerEye(), direction);
}

glm::vec3 CombatScene::TerminalPosition() const
{
    if (maintenanceMode)
    {
        return combat_map_asset::maintenanceTerminal;
    }
    return combatMapMode ? combat_map_asset::terminal : terminal_asset::position;
}

combat::Collider CombatScene::TerminalCollider() const
{
    auto box = terminal_asset::collider;
    const auto offset = TerminalPosition() - terminal_asset::position;
    box.minimum += offset;
    box.maximum += offset;
    return box;
}

glm::vec3 CombatScene::SupplyPosition() const
{
    if (maintenanceMode)
    {
        return combat_map_asset::maintenanceSupply;
    }
    return combatMapMode ? combat_map_asset::supply : combat::WeaponInventory::SupplyPosition();
}

std::vector<combat::Collider> CombatScene::PlayerObstacles() const
{
    if (bossMode)
    {
        std::vector<combat::Collider> walls(boss_asset::walls.begin(), boss_asset::walls.end());
        glm::vec3 low(std::numeric_limits<float>::max()), high(std::numeric_limits<float>::lowest());
        for (int i = 0; i < boss_asset::partCount; ++i)
        {
            const auto& part = boss_asset::parts[i];
            for (int corner = 0; corner < 8; ++corner)
            {
                const auto point =
                    glm::vec3(boss.Pose().parts[i] * glm::vec4(corner & 1 ? part.high.x : part.low.x,
                                                               corner & 2 ? part.high.y : part.low.y,
                                                               corner & 4 ? part.high.z : part.low.z, 1));
                low = glm::min(low, point);
                high = glm::max(high, point);
            }
        }
        walls.push_back({29000, combat::HitKind::Armor, low, high});
        return walls;
    }
    if (combatMapMode)
    {
        std::vector<combat::Collider> walls(combat_map_asset::playerWalls.begin(),
                                            combat_map_asset::playerWalls.end());
        walls.push_back(TerminalCollider());
        if (encounter.HasSupportTurret() && encounter.SupportTurret().Alive())
        {
            auto body = encounter.SupportTurret().Body();
            body.kind = combat::HitKind::Armor;
            walls.push_back(body);
        }
        return walls;
    }
    return turretMode ? encounter.Colliders()
                      : std::vector<combat::Collider>(range.Colliders().begin(), range.Colliders().end());
}

bool CombatScene::TerminalTarget() const
{
    return TerminalTarget(PlayerForward());
}

bool CombatScene::TerminalTarget(const glm::vec3& direction) const
{
    if (!corridorMode || bossMode)
    {
        return false;
    }
    return analysis.IsFirstTarget(
        TerminalCollider(), PlayerEye(), direction, encounter.Colliders(),
        Inventory().InteractionDistance(PlayerEye(), direction, encounter.Colliders()));
}

combat::WeaponInteraction CombatScene::InteractionHint() const
{
    if (bossMode)
    {
        return boss.InteractionHint(PlayerEye(), PlayerForward());
    }
    if (TerminalTarget())
    {
        return combat::WeaponInteraction::None;
    }
    return turretMode ? encounter.InteractionHint(PlayerEye(), PlayerForward())
                      : range.InteractionHint(PlayerEye(), PlayerForward());
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
    if (bossMode)
    {
        return boss.Health();
    }
    if (turretMode)
    {
        return encounter.Health();
    }
    return 100;
}

void CombatScene::UpdateBattle(double realDeltaTime, double gameDeltaTime)
{
    if (bossMode)
    {
        boss.Update(static_cast<float>(realDeltaTime), static_cast<float>(gameDeltaTime), PlayerEye(),
                    PlayerForward());
    }
    else if (turretMode)
    {
        encounter.Update(static_cast<float>(realDeltaTime), static_cast<float>(gameDeltaTime), PlayerEye(),
                         PlayerForward());
    }
    else
    {
        range.Update(static_cast<float>(realDeltaTime), static_cast<float>(gameDeltaTime));
    }
}

void CombatScene::UpdateVisualEffects(double realDeltaTime, double gameDeltaTime)
{
    destruction.UpdateWorld(static_cast<float>(gameDeltaTime));
    ObserveDestruction();
    destruction.ObserveHealth(PlayerHealth());
    if (IsDroneMode() && encounter.EnemyAlive() && encounter.Result() == combat::EncounterResult::Running)
    {
        drone.UpdateCombat(gameDeltaTime);
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
    const auto& state = encounter.DroneState();
    // 실습과 같은 순서로 위치 → 회전 → 크기를 적용한다.
    glm::mat4 droneWorld = glm::translate(glm::mat4(1), state.Position());
    droneWorld = glm::rotate(droneWorld, state.Yaw(), glm::vec3(0, 1, 0));
    droneWorld = glm::scale(droneWorld, glm::vec3(state.Scale()));
    destruction.ObserveEnemy(0, IsDroneMode() && encounter.EnemyAlive(), droneWorld);
    destruction.ObserveEnemy(1,
                             !bossMode && encounter.HasSupportTurret() && encounter.SupportTurret().Alive(),
                             glm::translate(glm::mat4(1), encounter.SupportTurret().Position()));
}

void CombatScene::Clear()
{
    analysis.Reset();
    ClearPlayerView();
    feedback.Reset();
    bossView.Clear();
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
