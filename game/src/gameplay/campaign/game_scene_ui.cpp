// 메뉴의 선택 결과를 캠페인 진행과 전투 장면에 전달한다.
#include "game_scene.h"

namespace aegis
{
void GameScene::SampleInput(GLFWwindow* window, bool canReceiveInput, bool fireClicked)
{
    bool consumed = false;
    const auto action = menu.SampleInput(window, canReceiveInput && !AnalysisActive(), Page(), consumed);
    ApplyAction(action);
    const bool combatInputActive = canReceiveInput && !consumed && (flow.WorldRuns() || AnalysisActive());
    combatScene.SampleInput(window, combatInputActive, fireClicked);
}

void GameScene::ApplyAction(UiAction action)
{
    if (action != UiAction::Credits && action != UiAction::None)
    {
        menu.ShowCredits(false);
    }
    switch (action)
    {
    case UiAction::Start:
        if (flow.Start())
        {
            LoadStage();
        }
        break;
    case UiAction::Credits:
        menu.ShowCredits(true);
        break;
    case UiAction::Help:
        flow.ShowHelp();
        break;
    case UiAction::Title:
        flow.ToTitle();
        LoadStage();
        break;
    case UiAction::Advance:
        if (ReadyToAdvance() && flow.Advance(true))
        {
            LoadStage();
        }
        break;
    case UiAction::Analyze:
        if (flow.Screen() == CampaignScreen::Playing && flow.Stage() == CampaignStage::Entry)
        {
            combatScene.OpenEntryAnalysis();
        }
        break;
    case UiAction::Resume:
        if (flow.Screen() == CampaignScreen::Paused)
        {
            flow.TogglePause();
        }
        break;
    case UiAction::Retry:
        if (flow.Retry())
        {
            LoadStage();
        }
        break;
    case UiAction::Restart:
        if (flow.Restart())
        {
            LoadStage();
        }
        break;
    case UiAction::Quit:
        quitRequested = true;
        break;
    case UiAction::None:
        break;
    }
}

bool GameScene::TogglePause()
{
    if (menu.CreditsVisible())
    {
        menu.ShowCredits(false);
        return true;
    }
    if (flow.Screen() == CampaignScreen::Help)
    {
        ApplyAction(UiAction::Title);
        return true;
    }
    return flow.TogglePause();
}

UiPage GameScene::Page() const
{
    auto page = menu.Page(flow, ReadyToAdvance());
    if (!menu.CreditsVisible() && flow.Screen() == CampaignScreen::Playing &&
        flow.Stage() != CampaignStage::Entry)
    {
        combatScene.PopulateHud(page);
    }
    return page;
}

std::string GameScene::StatusTitle() const
{
    const char* screen = "PLAYING";
    switch (flow.Screen())
    {
    case CampaignScreen::Title:   screen = "TITLE"; break;
    case CampaignScreen::Help:    screen = "HELP"; break;
    case CampaignScreen::Playing: break;
    case CampaignScreen::Paused:  screen = "PAUSED"; break;
    case CampaignScreen::Failed:  screen = "FAILED"; break;
    case CampaignScreen::Victory: screen = "VICTORY"; break;
    }
    if (menu.CreditsVisible())
    {
        screen = "CREDITS";
    }
    const char* stage = "ENTRY";
    switch (flow.Stage())
    {
    case CampaignStage::Entry:
        stage = "ENTRY";
        break;
    case CampaignStage::Corridor:
        stage = "CORRIDOR";
        break;
    case CampaignStage::Maintenance:
        stage = "MAINTENANCE";
        break;

    case CampaignStage::Boss:
        stage = "BOSS";
        break;
    }
    return std::string("AEGIS CAMPAIGN | ") + screen + " | " + stage + " | " +
           combatScene.StatusTitle(false) + combatScene.TimeDebug();
}

} // namespace aegis
