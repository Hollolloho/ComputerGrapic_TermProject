// 전투 장면: 입력은 combat_input.cpp, 그리기는 combat_render.cpp, 검사는 combat_smoke_tests.cpp.
#pragma once

#include "combat_range.h"
#include "player_camera.h"
#include "player_view.h"
#include "turret_encounter.h"
#include "weapon_view.h"
#include "drone.h"
#include "turret_view.h"
#include "analysis_session.h"
#include "boss_view.h"
#include "core_encounter.h"
#include "core_view.h"
#include "single_turret_view.h"
#include "korean_ui.h"
#include "world_signs.h"
#include "combat_feedback.h"
#include "destruction_effects.h"
#include "death_view.h"
#include "combat_time.h"
#include "combat_map_layout.generated.h"
#include <GL/glew.h>
#include <filesystem>
#include <string>
#include <functional>

struct GLFWwindow;
struct Camera;

namespace aegis
{

class CombatScene
{
public:
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
    void PlayerViewSmokeFrame(int frame);
    bool PlayerViewSmokePassed() const;
    void SampleInput(GLFWwindow* window, bool active);
    void Update(double realDeltaTime, double gameDeltaTime);
    void Draw(int width, int height,
              const std::function<void(const glm::mat4&, const glm::vec3&)>& drawEnvironment = {},
              bool showHud = true, const Camera* playerCamera = nullptr) const;
    void Clear();
    void SetSmokeFrame(int frame);
    bool SmokePassed() const;
    bool SaveFrame(const std::filesystem::path& path, int width, int height) const;

    const combat::CombatRange& Range() const
    {
        return range;
    }

    void SetTurretMode(bool enabled);
    void SetCorridorMode(bool enabled);
    void SetDroneMode(bool enabled);
    void SetCombatMapMode(bool enabled);

    void SetMaintenanceMode();

    bool IsCombatMapMode() const
    {
        return combatMapMode;
    }

    void SetBossMode(bool enabled);
    void SetCoreMode();
    bool IsCoreMode() const { return coreMode; }
    const combat::CoreEncounter& CoreState() const { return core; }

    void SetDebugControls(bool enabled)
    {
        debugControls = enabled;
    }

    combat::EncounterResult BattleResult() const
    {
        if (coreMode) return core.Result();
        if (bossMode)
        {
            return boss.Result();
        }
        return encounter.Result();
    }

    void CampaignSmokeFrame(int frame);
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
        if (movementTimeAvailable)
        {
            if (manualNormalTime)
            {
                return 1.0;
            }
            return movementScale;
        }
        if (slowPreview)
        {
            return 0.05;
        }
        return 1.0;
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
        return coreMode ? core.CoreHits() : boss.CoreHits();
    }

    bool IsBossMode() const
    {
        return bossMode;
    }

    bool IsDroneMode() const
    {
        return !coreMode && !bossMode && turretMode && encounter.IsDroneMode();
    }

    bool AnalysisActive() const
    {
        return analysis.Active();
    }

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
        return !coreMode && !bossMode && turretMode && encounter.CanAnalyze();
    }

    float DoorProgress() const
    {
        return corridorMode ? encounter.DoorProgress() : 0;
    }

    bool IsCorridorMode() const
    {
        return corridorMode;
    }

    bool IsTurretMode() const
    {
        return coreMode || bossMode || turretMode;
    }

    int SmokeFrameCount() const
    {
        if (combatMapMode)
        {
            return 1800;
        }
        if (bossMode)
        {
            return 2400;
        }
        if (IsDroneMode())
        {
            return 1320;
        }
        if (turretMode)
        {
            return 780;
        }
        return 300;
    }

    std::string StatusTitle(bool paused) const;

private:
    int PlayerHealth() const;
    void UpdateBattle(double realDeltaTime, double gameDeltaTime);
    void UpdateVisualEffects(double realDeltaTime, double gameDeltaTime);
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
    void DrawProjectiles(const glm::mat4& viewProjection) const;
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
    bool slowPreview{};
    bool previewHeld{};
    bool forceNormal{};
    bool movementTimeAvailable{};
    bool manualNormalTime{};
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
    SingleTurretView singleTurretView;
    bool coreMode{};
    bool debugControls{true};
    bool maintenanceMode{};
    bool combatMapMode{};
    bool smokeMapSlow{};
    bool smokeMapCoverBlocked{};
    bool smokeMapDeath{};
    bool smokeMapBoth{};
    bool smokeMapTurretShot{};
    bool smokeMapLocked{};
    bool bossMode{};
    bool bossCovered{};
    bool bossToggleHeld{};
    bool smokeBossFailureSeen{};
    combat::AnalysisSession analysis;
    std::vector<DrawMesh> terminalMeshes;
    combat::CombatRange range;
    combat::TurretEncounter encounter;
    PlayerCamera camera;
    PlayerViewInput externalView;
    bool smokeViewShot{};
    bool smokeViewSupply{};
    bool smokeViewPickup{};
    bool smokeViewAnalysis{};
    bool smokeViewRestored{};
    bool smokeViewReset{};
    bool smokeViewAim{};
    bool smokeViewDodge{};
    bool smokeViewDone{};
    PlayerView smokeViewSnapshot;
    WeaponView weapon;
    Drone drone;
    TurretView turretView;
    MeshRenderer terminalRenderer;
    GLuint program{};
    GLuint vao{};
    GLuint vbo{};
    GLint transformLocation{-1};
    GLint colorLocation{-1};
    GLint opacityLocation{-1};
    bool mouseHeld{};
    bool coverHeld{};
    bool lookHeld{};
    bool turretHeld{};
    bool droneToggleHeld{};
    bool smokePatrolSeen{};
    bool smokeWarningSeen{};
    bool smokeShotSeen{};
    bool smokeSecuringSeen{};
    bool smokeAnalysisSeen{};
    bool smokeAnalysisReturned{};
    bool smokeWorldFrozen{};
    float smokeDoorSnapshot{};
    bool dropHeld{};
    bool interactHeld{};
    bool turretMode{};
    bool corridorMode{};
    bool smokeFailureSeen{};
    double lastX{};
    double lastY{};
    float yaw{};
    float pitch{};
};

} // namespace aegis
