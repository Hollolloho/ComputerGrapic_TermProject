// 캠페인 진행 상태와 게임 장면의 생명주기를 연결한다.
#include "game_scene.h"

namespace aegis
{
bool GameScene::Init(const std::filesystem::path& projectDirectory)
{
    Clear();
    if (!environment.Init(projectDirectory) || !combatScene.Init(projectDirectory) ||
        !menu.Init(projectDirectory / "shaders"))
    {
        return false;
    }
    flow.ToTitle();
    LoadStage();
    return true;
}

void GameScene::Update(double realDeltaTime, double baseGameDeltaTime)
{
    if (!flow.WorldRuns())
    {
        return;
    }
    combatScene.Update(realDeltaTime, baseGameDeltaTime);
    const combat::EncounterResult result = combatScene.BattleResult();
    flow.ObserveBattle(result == combat::EncounterResult::Failed && !combatScene.DeathPlaying(),
                       result == combat::EncounterResult::Cleared);
}

void GameScene::Reset()
{
    const bool reset = flow.Screen() == CampaignScreen::Victory ? flow.Restart() : flow.Retry();
    if (reset)
    {
        LoadStage();
    }
}

void GameScene::Clear()
{
    combatScene.Clear();
    environment.Clear();
    menu.Clear();
}

void GameScene::LoadStage()
{
    ++playerSpawnRevision;
    switch (flow.Stage())
    {
    case CampaignStage::Entry:
        combatScene.SetCorridorMode(true);
        break;
    case CampaignStage::Corridor:
        combatScene.SetCombatMapMode(true);
        break;
    case CampaignStage::Maintenance:
        combatScene.SetMaintenanceMode();
        break;
    case CampaignStage::CoreApproach:
        combatScene.SetMaintenanceMode(true);
        break;
    case CampaignStage::Boss:
        combatScene.SetCoreMode();
        break;
    }
    environment.SetEntryCamera();
}

bool GameScene::ReadyToAdvance() const
{
    if (flow.Screen() != CampaignScreen::Playing || AnalysisActive())
    {
        return false;
    }
    if (flow.Stage() == CampaignStage::Entry)
    {
        return true;
    }
    if (flow.Stage() != CampaignStage::Corridor && flow.Stage() != CampaignStage::Maintenance &&
        flow.Stage() != CampaignStage::CoreApproach)
    {
        return false;
    }
    if (!combatScene.CanAnalyze())
    {
        return false;
    }
    if (combatScene.IsCombatMapMode())
    {
        return true;
    }
    return combatScene.DoorProgress() >= 1;
}

} // namespace aegis
