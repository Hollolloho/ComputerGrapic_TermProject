#pragma once

#include "campaign_environment.h"
#include "campaign_menu.h"
#include "gameplay/combat/combat_scene.h"
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

struct Camera;
struct GLFWwindow;

namespace aegis
{
// 캠페인 진행과 환경, 메뉴, 전투 장면을 연결한다.
class GameScene
{
public:
    GameScene() = default;
    GameScene(const GameScene&) = delete;
    GameScene& operator=(const GameScene&) = delete;

    bool Init(const std::filesystem::path& projectDirectory);
    void Update(double realDeltaTime, double baseGameDeltaTime);
    void Draw(int width, int height, const Camera* playerCamera = nullptr) const;

    void SampleInput(GLFWwindow* window, bool canReceiveInput, bool fireClicked);
    void Reset();
    void Clear();
    bool TogglePause();
    bool QuitRequested() const { return quitRequested; }
    std::string StatusTitle() const;

    bool SetPlayerView(const PlayerView& view);
    void ClearPlayerView();
    bool PlayerControlsActive() const;
    std::uint64_t PlayerSpawnRevision() const { return playerSpawnRevision; }
    bool HasExternalPlayerView() const;
    PlayerView CurrentPlayerView() const;
    std::vector<combat::Collider> PlayerObstacles() const;
    void SetMovementTimeScale(double scale);
    double PreviewTimeScale() const;
    bool AnalysisActive() const;
    bool CloseAnalysis();
    void ScrollAnalysis(double wheelSteps);
    const combat::AnalysisSession& AnalysisState() const;
    const CombatScene& Combat() const;

private:
    void LoadStage();
    bool ReadyToAdvance() const;
    void ApplyAction(UiAction action);
    UiPage Page() const;

    CampaignFlow flow;
    CampaignEnvironment environment;
    CampaignMenu menu;
    CombatScene combatScene;
    std::uint64_t playerSpawnRevision{};

    bool quitRequested{};
};
} // namespace aegis
