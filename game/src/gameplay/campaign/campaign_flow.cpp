#include "campaign_flow.h"

namespace aegis
{
bool CampaignFlow::Start()
{
    if (screen != CampaignScreen::Title)
    {
        return false;
    }
    stage = CampaignStage::Entry;
    screen = CampaignScreen::Playing;
    return true;
}

bool CampaignFlow::ShowHelp()
{
    if (screen != CampaignScreen::Title)
    {
        return false;
    }
    screen = CampaignScreen::Help;
    return true;
}

bool CampaignFlow::Advance(bool corridorSafe)
{
    if (screen != CampaignScreen::Playing)
    {
        return false;
    }
    if (stage == CampaignStage::Entry)
    {
        stage = CampaignStage::Corridor;
        return true;
    }
    if (stage == CampaignStage::Corridor && corridorSafe)
    {
        stage = CampaignStage::Maintenance;
        return true;
    }
    if (stage == CampaignStage::Maintenance && corridorSafe)
    {
        stage = CampaignStage::Boss;
        return true;
    }
    return false;
}

bool CampaignFlow::TogglePause()
{
    if (screen == CampaignScreen::Playing)
    {
        screen = CampaignScreen::Paused;
        return true;
    }
    if (screen == CampaignScreen::Paused)
    {
        screen = CampaignScreen::Playing;
        return true;
    }
    return false;
}

bool CampaignFlow::ObserveBattle(bool failed, bool cleared)
{
    if (!WorldRuns())
    {
        return false;
    }
    if (failed)
    {
        screen = CampaignScreen::Failed;
        return true;
    }
    if (cleared && stage == CampaignStage::Boss)
    {
        screen = CampaignScreen::Victory;
        return true;
    }
    return false;
}

bool CampaignFlow::Retry()
{
    if (screen != CampaignScreen::Failed && screen != CampaignScreen::Paused &&
        screen != CampaignScreen::Playing)
    {
        return false;
    }
    screen = CampaignScreen::Playing;
    return true;
}

bool CampaignFlow::Restart()
{
    if (screen != CampaignScreen::Victory && screen != CampaignScreen::Failed)
    {
        return false;
    }
    stage = CampaignStage::Entry;
    screen = CampaignScreen::Playing;
    return true;
}

void CampaignFlow::ToTitle()
{
    stage = CampaignStage::Entry;
    screen = CampaignScreen::Title;
}
} // namespace aegis
