// 환경, 전투와 메뉴 화면을 현재 진행 상태에 맞춰 연결한다.
#include "game_scene.h"
#include "campaign_environment.h"
#include "campaign_menu.h"
#include "gameplay/combat/combat_scene.h"

namespace aegis
{
void GameScene::DrawBattle(int width, int height, const Camera* playerCamera) const
{
    combatScene->Draw(
        width, height,
        [this](const glm::mat4& viewProjection, const glm::vec3& eye)
        {
            environment->DrawBattle(viewProjection, eye, combatScene->IsCombatMapMode(),
                                    combatScene->DoorProgress());
        },
        false, playerCamera);
}

void GameScene::Draw(int width, int height, const Camera* playerCamera) const
{
    if (width <= 0 || height <= 0)
    {
        return;
    }
    if (AnalysisActive())
    {
        DrawBattle(width, height, playerCamera);
        return;
    }
    if (flow.Stage() == CampaignStage::Entry || flow.Screen() == CampaignScreen::Title ||
        flow.Screen() == CampaignScreen::Help)
    {
        environment->DrawEntry(width, height);
    }
    else
    {
        DrawBattle(width, height, playerCamera);
    }
    if (!AnalysisActive())
    {
        menu->Draw(Page(), width, height);
    }
}

} // namespace aegis
