// 프레임워크의 플레이어 시점과 분석 상태를 전투 장면에 연결한다.
#include "game_scene.h"
#include "gameplay/combat/combat_scene.h"

namespace aegis
{
bool GameScene::SetPlayerView(const PlayerView& view)
{
    if (!flow.WorldRuns() || AnalysisActive())
    {
        return false;
    }
    return combatScene->SetPlayerView(view);
}

void GameScene::ClearPlayerView()
{
    combatScene->ClearPlayerView();
}

bool GameScene::PlayerControlsActive() const
{
    return flow.WorldRuns() && !AnalysisActive() && !combatScene->DeathStarted();
}

bool GameScene::HasExternalPlayerView() const
{
    return combatScene->HasExternalPlayerView();
}

PlayerView GameScene::CurrentPlayerView() const
{
    return combatScene->CurrentPlayerView();
}

void GameScene::SetMovementTimeScale(double scale)
{
    combatScene->SetMovementTimeScale(scale);
}

std::vector<combat::Collider> GameScene::PlayerObstacles() const
{
    return combatScene->PlayerObstacles();
}

double GameScene::PreviewTimeScale() const
{
    return combatScene->PreviewTimeScale();
}

bool GameScene::AnalysisActive() const
{
    return combatScene->AnalysisActive();
}

bool GameScene::CloseAnalysis()
{
    return combatScene->CloseAnalysis();
}

const combat::AnalysisSession& GameScene::AnalysisState() const
{
    return combatScene->AnalysisState();
}

const CombatScene& GameScene::Combat() const
{
    return *combatScene;
}
} // namespace aegis
