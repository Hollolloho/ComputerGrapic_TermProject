// 전투 장면의 입력, 전투 갱신과 화면 표시를 연결한다.
#pragma once

#include "gameplay/player/player_camera.h"
#include "gameplay/player/player_view.h"
#include "gameplay/turret/turret_encounter.h"
#include "gameplay/weapons/weapon_view.h"
#include "gameplay/weapons/bullet.h"
#include "drone/drone.h"
#include "gameplay/turret/turret_view.h"
#include "gameplay/ui/analysis_session.h"
#include "gameplay/boss/boss_view.h"
#include "gameplay/core/core_encounter.h"
#include "gameplay/core/core_view.h"
#include "gameplay/turret/turret.h"
#include "gameplay/ui/korean_ui.h"
#include "gameplay/ui/world_signs.h"
#include "combat_feedback.h"
#include "gameplay/effects/destruction_effects.h"
#include "gameplay/effects/death_view.h"
#include "combat_time.h"
#include "combat_map/combat_map_layout.generated.h"
#include <GL/glew.h>
#include <filesystem>
#include <string>

struct GLFWwindow;
struct Camera;

namespace aegis
{

class CampaignEnvironment;

class CombatScene
{
public:
    CombatScene() = default;
    CombatScene(const CombatScene&) = delete;
    CombatScene& operator=(const CombatScene&) = delete;

    bool Init(const std::filesystem::path& projectDirectory);
    void Reset();
    bool SetPlayerView(const PlayerView& view);

    void ClearPlayerView()
    {
        if (analysis.Active())
        {
            return;
        }
        externalView.Clear();
        lookHeld = false;
    }

    bool HasExternalPlayerView() const
    {
        return externalView.Active();
    }

    PlayerView CurrentPlayerView() const;
    void SampleInput(GLFWwindow* window, bool canReceiveInput, bool fireClicked);
    void Update(double realDeltaTime, double gameDeltaTime, float strafeAmount = 0.f);
    void Draw(int width, int height,
              const CampaignEnvironment* environment = nullptr,
              bool showHud = true, const Camera* playerCamera = nullptr) const;
    void Clear();

    void SetCorridorMode(bool enabled);
    void SetDroneMode(bool enabled);
    void SetCombatMapMode(bool enabled);

    void SetMaintenanceMode(bool coreApproach = false);

    bool IsCombatMapMode() const
    {
        return combatMapMode;
    }

    void SetBossMode(bool enabled);
    void SetCoreMode();
    bool IsCoreMode() const { return activeBattle == &core; }
    const combat::CoreEncounter& CoreState() const { return core; }

    combat::EncounterResult BattleResult() const;

    void PopulateHud(UiPage& page) const;

    bool DeathStarted() const
    {
        return destruction.DeathStarted();
    }

    bool DeathPlaying() const
    {
        return destruction.DeathPlaying();
    }

    const combat::DestructionEffects& Destruction() const
    {
        return destruction;
    }

    const combat::CombatFeedback& Feedback() const
    {
        return feedback;
    }

    double PreviewTimeScale() const
    {
        return movementTimeAvailable ? movementScale : 1.0;
    }

    void SetMovementTimeScale(double scale)
    {
        if (std::isfinite(scale) && scale >= 0.05 && scale <= 1)
        {
            movementScale = scale;
            movementTimeAvailable = true;
        }
    }

    std::vector<combat::Collider> PlayerObstacles() const;
    std::wstring TimeStatus() const;
    std::string TimeDebug() const;

    const combat::CombatTime& TimeState() const
    {
        return time;
    }

    int BossCoreHits() const
    {
        return IsCoreMode() ? core.CoreHits() : boss.CoreHits();
    }

    bool IsBossMode() const
    {
        return activeBattle == &boss;
    }

    bool IsDroneMode() const
    {
        return activeBattle == &encounter && encounter.IsDroneMode();
    }

    bool AnalysisActive() const
    {
        return analysis.Active();
    }

    void OpenEntryAnalysis() { analysis.Open(); }
    void ScrollAnalysis(double wheelSteps) { analysis.Zoom(wheelSteps); }

    bool CloseAnalysis()
    {
        return analysis.Close();
    }

    const combat::AnalysisSession& AnalysisState() const
    {
        return analysis;
    }

    bool CanAnalyze() const
    {
        return activeBattle == &encounter && encounter.CanAnalyze();
    }

    float DoorProgress() const
    {
        return corridorMode ? encounter.DoorProgress() : 0;
    }

    bool IsCorridorMode() const
    {
        return corridorMode;
    }

    std::string StatusTitle(bool paused) const;

private:
    void BeginCombatMap();
    int PlayerHealth() const;
    void UpdateBattle(double realDeltaTime, double gameDeltaTime);
    void UpdateVisualEffects(double realDeltaTime, double gameDeltaTime, float strafeAmount);
    bool Fire(const glm::vec3& direction);
    glm::vec3 CameraRoot() const;

    glm::vec3 PlayerEye() const
    {
        return CurrentPlayerView().eye;
    }

    glm::vec3 PlayerForward() const
    {
        return CurrentPlayerView().forward;
    }

    glm::mat4 PlayerViewMatrix() const
    {
        return CurrentPlayerView().ViewMatrix();
    }

    void ToggleBossCover();
    void DrawEnemies(const glm::mat4& viewProjection) const;
    void DrawCollisionBoxes(const glm::mat4& viewProjection) const;
    void DrawBullets(const glm::mat4& projection, const glm::mat4& view) const;
    void DrawWeaponFragments(const glm::mat4& viewProjection) const;
    void DrawBossHud(int width, int height) const;
    void DrawDamageFeedback(int width, int height) const;
    bool InitTerminal(const std::filesystem::path& root);
    glm::vec3 TerminalPosition() const;
    combat::Collider TerminalCollider() const;
    glm::vec3 SupplyPosition() const;
    bool TerminalTarget() const;
    bool TerminalTarget(const glm::vec3& direction) const;
    void DrawAnalysis(int width, int height) const;
    const combat::WeaponInventory& Inventory() const;
    bool Interact(const glm::vec3& direction);
    combat::WeaponInteraction InteractionHint() const;
    void DrawHud(int width, int height) const;
    void DrawBox(const glm::mat4& viewProjection, const glm::vec3& minimum, const glm::vec3& maximum,
                 const glm::vec3& color) const;
    combat::CombatTime time;
    bool forceNormal{};
    bool movementTimeAvailable{};
    double movementScale{0.05};
    combat::CombatFeedback feedback;
    combat::DestructionEffects destruction;
    DeathView deathView;
    WorldSigns signs;
    void ObserveDestruction();
    combat::BossEncounter boss;
    BossView bossView;
    combat::CoreEncounter core;
    CoreView coreView;
    Turret singleModel, twinModel;
    bool maintenanceMode{};
    bool approachMode{};
    bool combatMapMode{};
    bool bossCovered{};
    combat::AnalysisSession analysis;
    std::vector<DrawMesh> terminalMeshes;
    combat::TurretEncounter encounter;
    // 현재 구역에서 입력과 갱신을 받을 전투 객체를 가리킨다.
    combat::CombatEncounter* activeBattle = &encounter;
    PlayerCamera camera;
    PlayerViewInput externalView;
    WeaponView weapon;
    BulletView bulletView;
    Drone drone;
    TurretView turretView;
    MeshRenderer terminalRenderer;
    GLuint program{};
    GLuint vao{};
    GLuint vbo{};
    GLint transformLocation{-1};
    GLint colorLocation{-1};
    GLint opacityLocation{-1};
    bool coverHeld{};
    bool lookHeld{};
    bool analysisDragHeld{};
    bool dropHeld{};
    bool interactHeld{};
    bool corridorMode{};
    double lastX{};
    double lastY{};
    float rotY{};
    float rotX{};
};

} // namespace aegis
