#pragma once

#include "drone.h"
#include "player_camera.h"
#include "combat_test_scene.h"
#include "campaign_flow.h"
#include "korean_ui.h"

#include <array>
#include <filesystem>

namespace aegis {

class GameScene {
public:
    bool Init(const std::filesystem::path& projectDirectory, bool combatTest = false, bool turretTest = false, bool droneTest = true, bool bossTest = false, bool campaign = true,bool combatMapTest=false);
    void Update(double realDt,double baseGameDt);
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
    int SmokeFrameCount() const { return campaignMode ? 3000:testScene.SmokeFrameCount(); }
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
    bool InitCombatMap(const std::filesystem::path& root);
    void DrawEntry(int width,int height) const;
    void DrawBattle(int width,int height) const;
    void LoadStage();
    void ApplyAction(UiAction action);
    UiPage Page() const;
    bool ReadyToAdvance() const;
    bool InitCorridor(const std::filesystem::path& projectDirectory);

    CampaignFlow flow;
    bool campaignMode{},quitRequested{},mouseHeld{},enterHeld{},escapeHeld{};
    float mouseX{},mouseY{};
    int stageSmokeFrame{},failedSmokeFrame{},victorySmokeFrame{},pausedSmokeFrame{};
    bool smokeRetried{},smokePaused{},smokeVictory{},smokeRestarted{},smokeDone{};
    mutable KoreanUi ui;
    std::vector<DrawMesh> entryRoom,entryTerminal,combatMap;
    MeshRenderer renderer;
    std::array<CorridorObject, 5> corridor;
    Drone drone;
    PlayerCamera camera;
    CombatTestScene testScene;
    bool legacyTest{};
};

} // namespace aegis
