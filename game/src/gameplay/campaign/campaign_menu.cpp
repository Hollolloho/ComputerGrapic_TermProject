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

int FirstEnabledButton(const UiPage& page)
{
    for (int i = 0; i < static_cast<int>(page.buttons.size()); ++i)
        if (page.buttons[i].enabled)
            return i;
    return -1;
}

int NextEnabledButton(const UiPage& page, int current, int direction)
{
    const int count = static_cast<int>(page.buttons.size());
    for (int i = 0; i < count; ++i)
    {
        current = (current + direction + count) % count;
        if (page.buttons[current].enabled)
            return current;
    }
    return -1;
}
} // namespace

bool CampaignMenu::Init(const std::filesystem::path& shaderDirectory)
{
    return ui.Init(shaderDirectory);
}

void CampaignMenu::Draw(const UiPage& page, int width, int height) const
{
    auto drawPage = page;
    drawPage.focus = focusedPage == page.title ? focusedButton : FirstEnabledButton(page);
    ui.Draw(drawPage, width, height, mouseX, mouseY);
}

void CampaignMenu::Clear()
{
    ui.Clear();
    mouseHeld = enterHeld = upHeld = downHeld = tabHeld = false;
    focusedButton = -1;
    focusedPage.clear();
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
    const bool up = glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS;
    const bool down = glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS;
    const bool tab = glfwGetKey(window, GLFW_KEY_TAB) == GLFW_PRESS;
    const bool shift = glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ||
                       glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS;
    if (focusedPage != page.title || focusedButton < 0 ||
        focusedButton >= static_cast<int>(page.buttons.size()) || !page.buttons[focusedButton].enabled)
    {
        focusedPage = page.title;
        focusedButton = FirstEnabledButton(page);
    }
    consumed = false;
    UiAction selected = UiAction::None;
    if (active)
    {
        const bool previous = (up && !upHeld) || (tab && !tabHeld && shift);
        const bool next = (down && !downHeld) || (tab && !tabHeld && !shift);
        if ((previous || next) && focusedButton >= 0)
        {
            focusedButton = NextEnabledButton(page, focusedButton, previous ? -1 : 1);
            consumed = true;
        }
        bool pointerConsumed = false;
        const auto action = KoreanUi::Hit(page, mouseX, mouseY, pointerConsumed);
        const bool overUi = pointerConsumed;
        consumed = consumed || (left && overUi);
        if (left && !mouseHeld && overUi)
        {
            selected = action;
            for (int i = 0; i < static_cast<int>(page.buttons.size()); ++i)
                if (page.buttons[i].enabled && page.buttons[i].Contains(mouseX, mouseY))
                    focusedButton = i;
        }
        else if (enter && !enterHeld && focusedButton >= 0)
        {
            consumed = true;
            selected = page.buttons[focusedButton].action;
        }
    }
    mouseHeld = left;
    enterHeld = enter;
    upHeld = up;
    downHeld = down;
    tabHeld = tab;
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
        page.footer = L"Esc 돌아가기 · Enter 선택";
        return page;
    }
    switch (flow.Screen())
    {
    case CampaignScreen::Title:
        page.title = L"AEGIS";
        page.body = L"중앙 AI 코어를 파괴하고 시설 통제를 끊으세요.";
        AddMenuButton(page, UiAction::Start, L"게임 시작", 430);
        AddMenuButton(page, UiAction::Help, L"조작 안내", 494);
        AddMenuButton(page, UiAction::Quit, L"게임 종료", 558);
        page.buttons.push_back({UiAction::Credits, L"제작·출처", 650, 155, 156, 42, true});
        page.footer = L"↑↓ / Tab 선택 이동 · Enter 선택 · 마우스 클릭";
        break;
    case CampaignScreen::Help:
        page.title = L"조작 안내";
        page.body = L"WASD 이동 · 마우스 시점 · 좌클릭 사격\n"
                    L"정지 5% · 실제 이동량에 따라 시간 진행\n"
                    L"사격 후 0.25초 정상 진행 · Shift 정상 진행\n"
                    L"유효타에 내 총 파손 · 빈손으로 조준 + E 재무장\n"
                    L"G 내려놓기 · R 구역 재시작 · Esc 일시정지\n"
                    L"적·적탄 정리 후 Enter 다음 구역\n"
                    L"E 분석 · 드래그 공전 · 휠 확대 · E/Esc 닫기";
        AddMenuButton(page, UiAction::Title, L"시작 화면으로", 558);
        page.footer = L"↑↓ / Tab 선택 이동 · Enter 선택 · Esc 돌아가기";
        break;
    case CampaignScreen::Paused:
        page.title = L"일시정지";
        page.body = L"세계 시간 정지\n현재 구역을 다시 시작하면 전투와 장비가 초기화됩니다.";
        AddMenuButton(page, UiAction::Resume, L"계속하기", 414, 44);
        AddMenuButton(page, UiAction::Retry, L"현재 구역 다시 시작", 466, 44);
        AddMenuButton(page, UiAction::Title, L"시작 화면으로", 518, 44);
        AddMenuButton(page, UiAction::Quit, L"게임 종료", 570, 44);
        page.footer = L"Esc / Space 계속하기 · ↑↓ 선택 · Enter 선택";
        break;
    case CampaignScreen::Failed:
        page.title = L"임무 실패";
        if (flow.Stage() == CampaignStage::Interlock)
        {
            page.body = L"다시 시도하면 사각방 입구부터 시작합니다.";
        }
        else if (flow.Stage() == CampaignStage::Maintenance)
        {
            page.body = L"다시 시도하면 정비 복도 입구부터 시작합니다.";
        }
        else
        {
            page.body =
                flow.Stage() == CampaignStage::Boss
                    ? L"다시 시도하면 코어실 입구부터 시작합니다."
                    : L"다시 시도하면 첫 복도 입구부터 시작합니다.";
        }
        AddMenuButton(page, UiAction::Retry, L"현재 구역 다시 시작", 430);
        AddMenuButton(page, UiAction::Title, L"시작 화면으로", 494);
        AddMenuButton(page, UiAction::Quit, L"게임 종료", 558);
        page.footer = L"R 재시작 · ↑↓ 선택 · Enter 선택";
        break;
    case CampaignScreen::Victory:
        page.title = L"작전 성공";
        page.body = L"중앙 AI 코어를 파괴했습니다.\n다시 시작하면 안전 진입실로 돌아갑니다.";
        AddMenuButton(page, UiAction::Restart, L"처음부터 다시 시작", 430);
        AddMenuButton(page, UiAction::Title, L"시작 화면으로", 494);
        AddMenuButton(page, UiAction::Quit, L"게임 종료", 558);
        page.footer = L"R 처음부터 다시 시작 · ↑↓ 선택 · Enter 선택";
        break;
    case CampaignScreen::Playing:
        page.panel = false;
        if (flow.Stage() == CampaignStage::Entry)
        {
            page.title = L"안전 진입실";
            page.body = L"드론을 분석하거나 첫 복도로 진입하세요.";
            page.buttons.push_back({UiAction::Advance, L"복도로 진입", 744, 616, 232, 44, true});
            page.buttons.push_back({UiAction::Analyze, L"드론 분석", 744, 668, 232, 44, true});
            page.footer = L"↑↓ / Tab 선택 · Enter 확인";
        }
        else if (flow.Stage() == CampaignStage::Corridor)
        {
            page.title = L"01  첫 복도";
            page.body = L"드론과 포탑을 제거하세요.";
            if (readyToAdvance)
                page.buttons.push_back({UiAction::Advance, L"Enter · 사각방으로", 744, 640, 232, 54, true});
        }
        else if (flow.Stage() == CampaignStage::Interlock)
        {
            page.title = L"02  사각방";
            page.body = L"드론·포탑과 E형을 제거하세요.";
            if (readyToAdvance)
                page.buttons.push_back({UiAction::Advance, L"Enter · 정비 복도로", 744, 640, 232, 54, true});
        }
        else if (flow.Stage() == CampaignStage::Maintenance)
        {
            page.title = L"03  정비 복도";
            page.body = L"복도를 확보하고 코어실로 진입하세요.";
            if (readyToAdvance)
                page.buttons.push_back({UiAction::Advance, L"Enter · 코어실로", 744, 640, 232, 54, true});
        }

        else
        {
            page.title = L"04  코어실";
            page.body = L"중앙 AI 코어를 파괴하세요.";
        }
        break;
    }
    return page;
}

} // namespace aegis
