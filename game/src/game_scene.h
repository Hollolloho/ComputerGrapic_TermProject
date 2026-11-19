#pragma once
#include "CombatMap.h"
#include "game_options.h"

#include "drone.h"
#include "player_camera.h"
#include "combat_scene.h"
#include "campaign_flow.h"
#include "korean_ui.h"

#include <array>
#include <filesystem>
#include <cstdint>

namespace aegis
{

class GameScene
{
public:
    bool Init(const std::filesystem::path& projectDirectory, const GameRunOptions& options = {});
    void Update(double realDeltaTime, double baseGameDeltaTime);

    bool SetPlayerView(const PlayerView& view)
    {
        if ((campaignMode && !flow.WorldRuns()) || AnalysisActive())
        {
            return false;
        }
        return combatScene.SetPlayerView(view);
    }

    void ClearPlayerView()
    {
        combatScene.ClearPlayerView();
    }

    bool PlayerControlsActive() const
    {
        return (!campaignMode || flow.WorldRuns()) && !AnalysisActive() && !combatScene.DeathStarted();
    }

    std::uint64_t PlayerSpawnRevision() const
    {
        return playerSpawnRevision;
    }

    bool HasExternalPlayerView() const
    {
        return combatScene.HasExternalPlayerView();
    }

    PlayerView CurrentPlayerView() const
    {
        return combatScene.CurrentPlayerView();
    }

    void SetMovementTimeScale(double scale)
    {
        combatScene.SetMovementTimeScale(scale);
    }

    std::vector<combat::Collider> PlayerObstacles() const
    {
        return combatScene.PlayerObstacles();
    }

    double PreviewTimeScale() const
    {
        return combatScene.PreviewTimeScale();
    }

    void Draw(int width, int height) const;
    void Reset();
    void Clear();
    void ToggleMode();
    void SampleInput(GLFWwindow* window, bool active);

    bool AnalysisActive() const
    {
        return combatScene.AnalysisActive();
    }

    bool CloseAnalysis()
    {
        return combatScene.CloseAnalysis();
    }

    const combat::AnalysisSession& AnalysisState() const
    {
        return combatScene.AnalysisState();
    }

    bool IsCampaign() const
    {
        return campaignMode;
    }

    bool TogglePause();

    bool QuitRequested() const
    {
        return quitRequested;
    }

    std::string StatusTitle(bool paused) const;

    int SmokeFrameCount() const
    {
        return playerViewTestMode ? 480 : campaignMode ? 3000 : combatScene.SmokeFrameCount();
    }

    bool SmokePassed() const;

    bool IsCombatTest() const
    {
        return true;
    }

    const CombatScene& Combat() const
    {
        return combatScene;
    }

    void SmokeFrame(int frame);

private:
    struct CorridorObject
    {
        glm::mat4 model{1.0f};
        std::vector<DrawMesh> meshes;
    };

    bool InitEntry(const std::filesystem::path& root);
    void DrawEntry(int width, int height) const;
    void DrawBattle(int width, int height) const;
    void LoadStage();
    void ApplyAction(UiAction action);
    UiPage Page() const;
    bool ReadyToAdvance() const;
    bool InitCorridor(const std::filesystem::path& projectDirectory);

    std::uint64_t playerSpawnRevision{};
    CampaignFlow flow;
    bool playerViewTestMode{};
    bool showCredits{};
    bool campaignMode{};
    bool quitRequested{};
    bool mouseHeld{};
    bool enterHeld{};
    bool escapeHeld{};
    float mouseX{};
    float mouseY{};
    int stageSmokeFrame{};
    int failedSmokeFrame{};
    int victorySmokeFrame{};
    int pausedSmokeFrame{};
    bool smokeRetried{};
    bool smokePaused{};
    bool smokeVictory{};
    bool smokeRestarted{};
    bool smokeDone{};
    mutable KoreanUi ui;
    std::vector<DrawMesh> entryRoom, entryTerminal;
    CombatMap combatMap;
    MeshRenderer renderer;
    std::array<CorridorObject, 5> corridor;
    Drone drone;
    PlayerCamera camera;
    CombatScene combatScene;
    bool legacyTest{};
};

} // namespace aegis
