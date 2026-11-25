#include "campaign_menu.h"
#include <GLFW/glfw3.h>

namespace aegis
{
namespace
{
void AddMenuButton(UiPage& page, UiAction action, const wchar_t* label, float y, float height = 52)
{
    page.buttons.push_back({action, label, 218, y, 588, height, true});
}
} // namespace

bool CampaignMenu::Init(const std::filesystem::path& shaderDirectory)
{
    return ui.Init(shaderDirectory);
}

void CampaignMenu::Draw(const UiPage& page, int width, int height) const
{
    ui.Draw(page, width, height, mouseX, mouseY);
}

void CampaignMenu::Clear()
{
    ui.Clear();
}

UiAction CampaignMenu::SampleInput(GLFWwindow* window, bool active, const UiPage& page, bool& consumed)
{
    double x{}, y{};
    int width{}, height{};
    glfwGetCursorPos(window, &x, &y);
    glfwGetWindowSize(window, &width, &height);
    mouseX = width > 0 ? static_cast<float>(x) * 1024 / width : 0;
    mouseY = height > 0 ? static_cast<float>(y) * 768 / height : 0;
    if (glfwGetInputMode(window, GLFW_CURSOR) == GLFW_CURSOR_DISABLED)
    {
        mouseX = mouseY = -1;
    }
    const bool left = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
    const bool enter = glfwGetKey(window, GLFW_KEY_ENTER) == GLFW_PRESS;
    consumed = false;
    UiAction selected = UiAction::None;
    if (active)
    {
        const auto action = KoreanUi::Hit(page, mouseX, mouseY, consumed);
        const bool overUi = consumed;
        consumed = left && overUi;
        if (left && !mouseHeld && overUi)
        {
            selected = action;
        }
        else if (enter && !enterHeld && !page.buttons.empty())
        {
            consumed = true;
            if (page.buttons[0].enabled)
            {
                selected = page.buttons[0].action;
            }
        }
    }
    mouseHeld = left;
    enterHeld = enter;
    return selected;
}

UiPage CampaignMenu::Page(const CampaignFlow& flow, bool readyToAdvance) const
{
    UiPage page;
    if (showCredits)
    {
        page.title = L"제작 · 출처";
        page.body = L"제작: 홍준호 · 윤형균\n시간 전투 참고: SUPERHOT\n라이브러리: GLEW · GLFW · GLM · "
                    L"Assimp\n한글: Windows 맑은 고딕 (폰트 파일 미배포)\n모델·텍스처 제작: Codex · Tripo AI "
                    L"활용\n라이선스 원문: external 폴더의 각 라이브러리";
        page.buttons.push_back({UiAction::Title, L"시작 화면으로", 218, 558, 588, 52, true});
        page.footer = L"Esc: 돌아가기";
        return page;
    }
    switch (flow.Screen())
    {
    case CampaignScreen::Title:
        page.title = L"AEGIS  저항 작전";
        page.body = L"중앙 AI가 장악한 시설에 침투하세요.\n전투 복도를 돌파하고 코어를 파괴하는 것이 "
                    L"목표입니다.\n\n시작 → 안전 진입실 → 첫 복도 → 정비 복도 → 코어실 앞 복도 → 코어실";
        AddMenuButton(page, UiAction::Start, L"게임 시작", 430);
        AddMenuButton(page, UiAction::Help, L"조작 안내", 494);
        AddMenuButton(page, UiAction::Quit, L"게임 종료", 558);
        page.buttons.push_back({UiAction::Credits, L"제작·출처", 650, 155, 156, 42, true});
        page.footer = L"Enter: 게임 시작  ·  마우스로 버튼을 선택할 수 있습니다.";
        break;
    case CampaignScreen::Help:
        page.title = L"조작 안내";
        page.body = L"WASD 이동 · 마우스 조준 · 왼쪽 클릭 사격\n정지: 느린 시간 / 이동: 시간 진행\n사격 후 "
                    L"0.25초는 적도 정상 진행\n본체 명중 → 내 총 파손 / 빈손 + E: 재무장\n모든 적과 적탄 "
                    L"정리 → 다음 구역\nShift 정상 진행 · R 재시작 · Esc 메뉴\n보스: 코어를 공격하고 50% 이하 자기장을 피하세요.";
        AddMenuButton(page, UiAction::Title, L"시작 화면으로", 558);
        page.footer = L"G: 총 내려놓기 · E: 회수·보급·분석 · Enter: 안전 확보 후 다음 구역";
        break;
    case CampaignScreen::Paused:
        page.title = L"일시정지";
        page.body = L"전투와 세계 시간이 멈췄습니다.\n계속 진행하거나 현재 구역을 다시 시작할 수 있습니다.";
        AddMenuButton(page, UiAction::Resume, L"계속하기", 414, 44);
        AddMenuButton(page, UiAction::Retry, L"현재 구역 다시 시작", 466, 44);
        AddMenuButton(page, UiAction::Title, L"시작 화면으로", 518, 44);
        AddMenuButton(page, UiAction::Quit, L"게임 종료", 570, 44);
        page.footer = L"Esc 또는 Space: 계속하기";
        break;
    case CampaignScreen::Failed:
        page.title = L"임무 실패";
        if (flow.Stage() == CampaignStage::CoreApproach)
        {
            page.body = L"코어실 앞 복도에서 전투 불능이 되었습니다. 이 복도부터 다시 시작합니다.";
        }
        else if (flow.Stage() == CampaignStage::Maintenance)
        {
            page.body = L"정비 복도에서 전투 불능이 되었습니다.\n다시 시도하면 정비 복도부터 시작합니다.";
        }
        else
        {
            page.body =
                flow.Stage() == CampaignStage::Boss
                    ? L"코어실에서 전투 불능이 되었습니다.\n다시 시도하면 보스전부터 시작합니다."
                    : L"전투 복도에서 전투 불능이 되었습니다.\n다시 시도하면 복도 전투부터 시작합니다.";
        }
        AddMenuButton(page, UiAction::Retry, L"현재 구역 다시 시작", 430);
        AddMenuButton(page, UiAction::Title, L"시작 화면으로", 494);
        AddMenuButton(page, UiAction::Quit, L"게임 종료", 558);
        page.footer = L"R 또는 Enter: 현재 구역 다시 시작";
        break;
    case CampaignScreen::Victory:
        page.title = L"작전 성공";
        page.body = L"중앙 AI의 코어를 파괴했습니다.\n시설 침투 작전을 완료했습니다.\n\n다시 시작하면 안전 "
                    L"진입실부터 진행합니다.";
        AddMenuButton(page, UiAction::Restart, L"처음부터 다시 시작", 430);
        AddMenuButton(page, UiAction::Title, L"시작 화면으로", 494);
        AddMenuButton(page, UiAction::Quit, L"게임 종료", 558);
        page.footer = L"R 또는 Enter: 처음부터 다시 시작";
        break;
    case CampaignScreen::Playing:
        page.panel = false;
        if (flow.Stage() == CampaignStage::Entry)
        {
            page.title = L"안전 진입실";
            page.body = L"이동: 시간 진행 · 본체 명중: 내 총 파손 · 빈손 + E: 재무장\n"
                        L"분석: 드래그 공전 · 휠 확대 · E/Esc 닫기";
            page.buttons.push_back({UiAction::Advance, L"Enter · 복도로 진입", 744, 616, 232, 44, true});
            page.buttons.push_back({UiAction::Analyze, L"드론 분석", 744, 668, 232, 44, true});
        }
        else if (flow.Stage() == CampaignStage::Corridor)
        {
            page.title = L"1스테이지 · 첫 복도";
            page.body = readyToAdvance
                            ? L"복도가 안전해졌습니다. 오른쪽 단말에서 분석하거나 정비 복도로 진행하세요."
                            : L"드론과 포탑을 제거하고 남은 적탄을 피하세요. 모든 위험이 사라지면 진행할 수 "
                              L"있습니다.";
            page.buttons.push_back(
                {UiAction::Advance, L"Enter · 정비 복도로", 744, 640, 232, 54, readyToAdvance});
        }
        else if (flow.Stage() == CampaignStage::Maintenance)
        {
            page.title = L"2스테이지 · 정비 복도";
            page.body = L"드론과 Single_C 포탑 두 대를 파괴하고 남은 적탄을 정리하세요.";
            page.buttons.push_back(
                {UiAction::Advance, L"Enter · 코어실 앞 복도로", 744, 640, 232, 54, readyToAdvance});
        }
        else if (flow.Stage() == CampaignStage::CoreApproach)
        {
            page.title = L"3스테이지 · 코어실 앞 복도";
            page.body = L"Twin 포탑을 파괴하고 코어실로 진입하세요.";
            page.buttons.push_back(
                {UiAction::Advance, L"Enter · 코어실로", 744, 640, 232, 54, readyToAdvance});
        }
        else
        {
            page.title = L"3스테이지 · 코어실 보스전";
            page.body = L"호위 드론과 포탑을 피하며 중앙 코어를 파괴하세요. 코어 체력 50% 이하에서는 "
                        L"자기장이 발동합니다.\n빈손일 때 입구 보급대 조준 + E  /  Esc로 메뉴";
        }
        break;
    }
    return page;
}

} // namespace aegis
