// 캠페인 진행 상태와 게임 장면의 생명주기를 연결한다.
#include "game_scene.h"
#include "campaign_environment.h"
#include "campaign_menu.h"
#include "gameplay/combat/combat_scene.h"

namespace aegis
{
GameScene::GameScene()
    : environment(std::make_unique<CampaignEnvironment>()),
      menu(std::make_unique<CampaignMenu>()),
      combatScene(std::make_unique<CombatScene>())
{
}

// GL 자원은 프레임워크가 컨텍스트를 닫기 전에 Clear에서 해제한다.
GameScene::~GameScene() = default;

bool GameScene::Init(const std::filesystem::path& projectDirectory)
{
    Clear();
    if (!environment->Init(projectDirectory) || !combatScene->Init(projectDirectory) ||
        !menu->Init())
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
    combatScene->Update(realDeltaTime, baseGameDeltaTime);
    flow.ObserveBattle(combatScene->BattleResult() == combat::EncounterResult::Failed &&
                               !combatScene->DeathPlaying(),
                           combatScene->BattleResult() == combat::EncounterResult::Cleared);
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
    combatScene->Clear();
    environment->Clear();
    menu->Clear();
}

void GameScene::LoadStage()
{
    ++playerSpawnRevision;
    combatScene->CloseAnalysis();
    combatScene->SetCorridorMode(true);
    combatScene->SetDroneMode(true);
    if (flow.Stage() == CampaignStage::Corridor)
    {
        combatScene->SetCombatMapMode(true);
    }
    if (flow.Stage() == CampaignStage::Maintenance)
    {
        combatScene->SetMaintenanceMode();
    }
    if (flow.Stage() == CampaignStage::Boss)
    {
        combatScene->SetCoreMode();
    }
    environment->SetEntryCamera();
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
    if (flow.Stage() != CampaignStage::Corridor && flow.Stage() != CampaignStage::Maintenance)
    {
        return false;
    }
    if (!combatScene->CanAnalyze())
    {
        return false;
    }
    if (combatScene->IsCombatMapMode())
    {
        return true;
    }
    return combatScene->DoorProgress() >= 1;
}

} // namespace aegis
