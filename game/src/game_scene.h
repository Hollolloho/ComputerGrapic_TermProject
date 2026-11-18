#pragma once
#include "CombatMap.h"

#include "drone.h"
#include "player_camera.h"
#include "combat_test_scene.h"
#include "campaign_flow.h"
#include "korean_ui.h"

#include <array>
#include <filesystem>
#include <cstdint>

namespace aegis {

class GameScene {
public:
    bool Init(const std::filesystem::path& projectDirectory, bool combatTest = false, bool turretTest = false, bool droneTest = true, bool bossTest = false, bool campaign = true,bool combatMapTest=false,bool playerViewTest=false);
    void Update(double realDt,double baseGameDt);
    bool SetPlayerView(const PlayerView& view) {
        if((campaignMode && !flow.WorldRuns()) || AnalysisActive()) return false;
        return testScene.SetPlayerView(view);
    }
    void ClearPlayerView() { testScene.ClearPlayerView(); }
    bool PlayerControlsActive() const { return (!campaignMode || flow.WorldRuns()) && !AnalysisActive() && !testScene.DeathStarted(); }
    std::uint64_t PlayerSpawnRevision() const { return playerSpawnRevision; }
    bool HasExternalPlayerView() const { return testScene.HasExternalPlayerView(); }
    PlayerView CurrentPlayerView() const { return testScene.CurrentPlayerView(); }
    void SetMovementTimeScale(double scale) { testScene.SetMovementTimeScale(scale); }
    std::vector<combat::Collider> PlayerObstacles() const { return testScene.PlayerObstacles(); }
    double PreviewTimeScale() const { return testScene.PreviewTimeScale(); }
    void Draw(int width, int height) const;
    void Reset();
    void Clear();
    void ToggleMode();
    void SampleInput(GLFWwindow* window, bool active);
    bool AnalysisActive() const { return testScene.AnalysisActive(); }
    bool CloseAnalysis() { return testScene.CloseAnalysis(); }
    const combat::AnalysisSession& AnalysisState() const { return testScene.AnalysisState(); }
    bool IsCampaign() const { return campaignMode; }
    bool TogglePause();
    bool QuitRequested() const { return quitRequested; }
    std::string StatusTitle(bool paused) const;
    int SmokeFrameCount() const { return playerViewTestMode ? 480:campaignMode ? 3000:testScene.SmokeFrameCount(); }
    bool SmokePassed() const;
    bool IsCombatTest() const { return true; }
    const CombatTestScene& TestScene() const { return testScene; }
    void SmokeFrame(int frame);

private:
    struct CorridorObject {
        glm::mat4 model{1.0f};
        std::vector<DrawMesh> meshes;
    };

    bool InitEntry(const std::filesystem::path& root);
    void DrawEntry(int width,int height) const;
    void DrawBattle(int width,int height) const;
    void LoadStage();
    void ApplyAction(UiAction action);
    UiPage Page() const;
    bool ReadyToAdvance() const;
    bool InitCorridor(const std::filesystem::path& projectDirectory);

    std::uint64_t playerSpawnRevision{};
    CampaignFlow flow;
    bool playerViewTestMode{};
    bool showCredits{};
    bool campaignMode{},quitRequested{},mouseHeld{},enterHeld{},escapeHeld{};
    float mouseX{},mouseY{};
    int stageSmokeFrame{},failedSmokeFrame{},victorySmokeFrame{},pausedSmokeFrame{};
    bool smokeRetried{},smokePaused{},smokeVictory{},smokeRestarted{},smokeDone{};
    mutable KoreanUi ui;
    std::vector<DrawMesh> entryRoom,entryTerminal;
    CombatMap combatMap;
    MeshRenderer renderer;
    std::array<CorridorObject, 5> corridor;
    Drone drone;
    PlayerCamera camera;
    CombatTestScene testScene;
    bool legacyTest{};
};

} // namespace aegis
