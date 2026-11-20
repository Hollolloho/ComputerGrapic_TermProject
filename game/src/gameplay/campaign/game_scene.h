#pragma once

#include "campaign_flow.h"
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

struct Camera;
struct GLFWwindow;

namespace aegis
{
class CampaignEnvironment;
class CampaignMenu;
class CombatScene;
struct PlayerView;
struct UiPage;
enum class UiAction;
namespace combat
{
class AnalysisSession;
struct Collider;
}

// 캠페인 진행과 환경, 메뉴, 전투 장면을 연결한다.
class GameScene
{
public:
    GameScene();
    ~GameScene();
    GameScene(const GameScene&) = delete;
    GameScene& operator=(const GameScene&) = delete;

    bool Init(const std::filesystem::path& projectDirectory);
    void Update(double realDeltaTime, double baseGameDeltaTime);
    void Draw(int width, int height, const Camera* playerCamera = nullptr) const;

    void SampleInput(GLFWwindow* window, bool active);
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
    void DrawBattle(int width, int height, const Camera* playerCamera) const;

    CampaignFlow flow;
    std::unique_ptr<CampaignEnvironment> environment;
    std::unique_ptr<CampaignMenu> menu;
    std::unique_ptr<CombatScene> combatScene;
    std::uint64_t playerSpawnRevision{};

    bool quitRequested{};
};
} // namespace aegis
