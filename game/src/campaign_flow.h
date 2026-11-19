#pragma once

namespace aegis
{
enum class CampaignStage
{
    Entry,
    Corridor,
    Maintenance,
    Boss
};
enum class CampaignScreen
{
    Title,
    Help,
    Playing,
    Paused,
    Failed,
    Victory
};

class CampaignFlow
{
public:
    bool Start();
    bool ShowHelp();
    bool Advance(bool corridorSafe);
    bool TogglePause();
    bool ObserveBattle(bool failed, bool cleared);
    bool Retry();
    bool Restart();
    void ToTitle();

    CampaignStage Stage() const
    {
        return stage;
    }

    CampaignScreen Screen() const
    {
        return screen;
    }

    bool WorldRuns() const
    {
        return screen == CampaignScreen::Playing && stage != CampaignStage::Entry;
    }

private:
    CampaignStage stage{CampaignStage::Entry};
    CampaignScreen screen{CampaignScreen::Title};
};
} // namespace aegis
