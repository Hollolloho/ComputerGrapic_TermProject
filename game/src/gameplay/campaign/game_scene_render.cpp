// 환경, 전투와 메뉴 화면을 현재 진행 상태에 맞춰 연결한다.
#include "game_scene.h"

namespace aegis
{
void GameScene::Draw(int width, int height, const Camera* playerCamera) const
{
    if (width <= 0 || height <= 0)
    {
        return;
    }
    if (AnalysisActive())
    {
        combatScene.Draw(width, height, &environment, false, playerCamera);
        return;
    }
    if (flow.Stage() == CampaignStage::Entry || flow.Screen() == CampaignScreen::Title ||
        flow.Screen() == CampaignScreen::Help)
    {
        environment.DrawEntry(width, height);
    }
    else
    {
        combatScene.Draw(width, height, &environment, false, playerCamera);
    }
    menu.Draw(Page(), width, height);
}

} // namespace aegis
