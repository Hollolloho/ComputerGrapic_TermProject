#pragma once

#include "campaign_flow.h"
#include "gameplay/ui/korean_ui.h"

struct GLFWwindow;

namespace aegis
{
// 메뉴 문구, 버튼 입력과 한글 화면 표시를 담당한다.
class CampaignMenu
{
public:
    bool Init(const std::filesystem::path& shaderDirectory = "shaders");
    UiPage Page(const CampaignFlow& flow, bool readyToAdvance) const;
    UiAction SampleInput(GLFWwindow* window, bool active, const UiPage& page, bool& consumed);
    void Draw(const UiPage& page, int width, int height) const;
    void Clear();

    void ShowCredits(bool show) { showCredits = show; }
    bool CreditsVisible() const { return showCredits; }

private:
    mutable KoreanUi ui;
    bool showCredits{};
    bool mouseHeld{};
    bool enterHeld{};
    bool upHeld{}, downHeld{}, tabHeld{};
    int focusedButton{-1};
    std::wstring focusedPage;
    float mouseX{};
    float mouseY{};
};
} // namespace aegis
