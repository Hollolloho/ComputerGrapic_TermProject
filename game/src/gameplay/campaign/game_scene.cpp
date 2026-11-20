// 시작 화면, 전투 구역, 일시정지와 결과 화면을 연결한다.
#include "game_scene.h"
#include "combat_map/corridor_layout.generated.h"
#include "campaign_layout.generated.h"
#include "combat_map/terminal_layout.generated.h"
#include <GLFW/glfw3.h>

#include <GL/glew.h>
#include <glm/gtc/matrix_transform.hpp>

#include <cmath>
#include <fstream>
#include <iostream>

namespace aegis
{
namespace
{

using corridor_asset::placements;

const glm::vec3 Up(0.0f, 1.0f, 0.0f);

} // namespace

bool GameScene::Init(const std::filesystem::path& projectDirectory, const GameRunOptions& options)
{
    Clear();
    legacyTest = options.combatTest;
    campaignMode = options.campaign;
    playerViewTestMode = options.playerViewTest;
    glEnable(GL_DEPTH_TEST);
    // 메시 셰이더가 최종 색을 sRGB로 변환한다.
    glDisable(GL_FRAMEBUFFER_SRGB);
    glClearColor(0.16f, 0.19f, 0.24f, 1.0f);
    if (!renderer.Init(projectDirectory / "shaders") || !InitCorridor(projectDirectory) ||
        !combatScene.Init(projectDirectory) || !InitEntry(projectDirectory) || !ui.Init())
    {
        return false;
    }
    if ((campaignMode || options.combatMapTest || options.playerViewTest) &&
        !combatMap.Init(projectDirectory))
    {
        return false;
    }
    combatScene.SetCorridorMode(!legacyTest);
    if (legacyTest)
    {
        combatScene.SetTurretMode(options.turretTest);
    }
    else
    {
        combatScene.SetDroneMode(options.droneTest);
    }
    if (options.bossTest)
    {
        combatScene.SetBossMode(true);
    }
    if (options.combatMapTest || options.playerViewTest)
    {
        combatScene.SetCombatMapMode(true);
    }
    combatScene.SetDebugControls(!campaignMode);
    flow.ToTitle();
    if (campaignMode)
    {
        LoadStage();
    }
    return true;
}

bool GameScene::InitCorridor(const std::filesystem::path& projectDirectory)
{
    const auto directory = projectDirectory / "asset" / "corridor" / "test_scene";
    const auto materialPath = directory / "materials.mtl";
    if (!std::ifstream(materialPath))
    {
        std::cerr << "Could not read corridor materials: " << materialPath << '\n';
        return false;
    }
    const ObjModelCache cache(projectDirectory / "build" / "cache" / "models", materialPath);

    static_assert(std::size(placements) == std::tuple_size_v<decltype(corridor)>);
    for (std::size_t i = 0; i < corridor.size(); ++i)
    {
        auto& object = corridor[i];
        const auto& placement = placements[i];
        const glm::mat4 parent = placement.parent >= 0 ? corridor[placement.parent].model : glm::mat4(1.0f);
        object.model = glm::rotate(glm::translate(parent, placement.translation),
                                   glm::radians(placement.yawDegrees), Up);

        const auto path = directory / placement.file;
        ModelData model;
        std::string error;
        bool cacheHit{};
        if (!cache.Load(path, model, error, cacheHit))
        {
            std::cerr << error << '\n';
            return false;
        }

        for (const auto& mesh : model.meshes)
        {
            const auto& material = model.materials[mesh.materialIndex];
            auto& drawMesh = object.meshes.emplace_back();
            drawMesh.Upload(mesh, material);
            // MTL의 반사색과 Phong 지수로 비금속 GGX 재질을 근사한다.
            drawMesh.dielectricF0 = glm::clamp(material.specular, 0.0f, 1.0f);
            drawMesh.roughness = std::sqrt(2.0f / (glm::max(material.shininess, 0.0f) + 2.0f));
        }
    }
    return true;
}

void GameScene::Update(double realDeltaTime, double baseGameDeltaTime)
{
    if (campaignMode && !flow.WorldRuns())
    {
        return;
    }
    combatScene.Update(realDeltaTime, baseGameDeltaTime);
    if (campaignMode)
    {
        flow.ObserveBattle(combatScene.BattleResult() == combat::EncounterResult::Failed &&
                               !combatScene.DeathPlaying(),
                           combatScene.BattleResult() == combat::EncounterResult::Cleared);
    }
}

void GameScene::DrawBattle(int width, int height, const Camera* playerCamera) const
{
    const bool showHud = !campaignMode;
    if (legacyTest)
    {
        combatScene.Draw(width, height, {}, showHud, playerCamera);
        return;
    }
    combatScene.Draw(
        width, height,
        [this](const glm::mat4& viewProjection, const glm::vec3& eye)
        {
            renderer.Begin(eye, {0.45f, 0.9f, 0.7f}, {1, 1, 1}, 2.5f, 0.45f);
            if (combatScene.IsCombatMapMode())
            {
                combatMap.Draw(viewProjection);
                return;
            }
            for (std::size_t i = 0; i < corridor.size(); ++i)
            {
                auto model = corridor[i].model;
                if (i == 2 || i == 3)
                {
                    model = glm::translate(model, glm::vec3((i == 2 ? -1 : 1) * corridor_asset::doorTravel *
                                                                combatScene.DoorProgress(),
                                                            0, 0));
                }
                renderer.Draw(corridor[i].meshes, viewProjection, model);
            }
        },
        showHud, playerCamera);
}

void GameScene::Reset()
{
    if (campaignMode)
    {
        const bool reset = flow.Screen() == CampaignScreen::Victory ? flow.Restart() : flow.Retry();
        if (reset)
        {
            LoadStage();
        }
        return;
    }
    ++playerSpawnRevision;
    camera.Reset();
    combatScene.Reset();
}

void GameScene::Clear()
{
    combatScene.Clear();
    for (auto& object : corridor)
    {
        for (auto& mesh : object.meshes)
        {
            mesh.Clear();
        }
        object.meshes.clear();
    }
    combatMap.Clear();
    for (auto* meshes : {&entryRoom, &entryTerminal})
    {
        for (auto& mesh : *meshes)
        {
            mesh.Clear();
        }
        meshes->clear();
    }
    ui.Clear();
    renderer.Clear();
}

void GameScene::ToggleMode()
{
    if (campaignMode)
    {
        return;
    }
    ++playerSpawnRevision;
    legacyTest = !legacyTest;
    combatScene.SetCorridorMode(!legacyTest);
    if (legacyTest)
    {
        combatScene.SetTurretMode(false);
    }
}

void GameScene::SampleInput(GLFWwindow* window, bool active)
{
    if (!campaignMode)
    {
        combatScene.SampleInput(window, active);
        return;
    }
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
    bool consumed = false;
    if (active && !AnalysisActive())
    {
        const auto page = Page();
        const auto action = KoreanUi::Hit(page, mouseX, mouseY, consumed);
        const bool overUi = consumed;
        consumed = left && overUi;
        if (left && !mouseHeld && overUi)
        {
            ApplyAction(action);
        }
        else if (enter && !enterHeld && !page.buttons.empty())
        {
            consumed = true;
            if (page.buttons[0].enabled)
            {
                ApplyAction(page.buttons[0].action);
            }
        }
    }
    const bool battle = flow.WorldRuns() && !consumed;
    combatScene.SampleInput(window, active && battle);
    mouseHeld = left;
    enterHeld = enter;
}

bool GameScene::InitEntry(const std::filesystem::path& root)
{
    const auto load = [&](const std::filesystem::path& path, std::vector<DrawMesh>& meshes)
    {
        ModelData model;
        std::string error;
        if (!LoadModel(path, model, error))
        {
            std::cerr << error << '\n';
            return false;
        }
        for (const auto& mesh : model.meshes)
        {
            auto& draw = meshes.emplace_back();
            draw.Upload(mesh, model.materials.at(mesh.materialIndex));
        }
        return true;
    };
    return load(root / campaign_asset::entryMesh, entryRoom) &&
           load(root / terminal_asset::mesh, entryTerminal);
}

void GameScene::DrawEntry(int width, int height) const
{
    glViewport(0, 0, width, height);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_FRAMEBUFFER_SRGB);
    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glDisable(GL_CULL_FACE);
    glClearColor(0.03f, 0.06f, 0.09f, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    const auto viewProjection =
        glm::perspective(glm::radians(60.0f), static_cast<float>(width) / height, 0.05f, 80.0f) *
        camera.ViewMatrix();
    renderer.Begin(camera.EyePosition(), {0.45f, 0.9f, 0.7f}, {1, 1, 1}, 3, 0.35f);
    renderer.Draw(entryRoom, viewProjection, glm::mat4(1));
    for (const auto& object : corridor)
    {
        renderer.Draw(object.meshes, viewProjection, object.model);
    }
    renderer.Draw(entryTerminal, viewProjection,
                  glm::rotate(glm::translate(glm::mat4(1), campaign_asset::terminalPosition),
                              glm::radians(campaign_asset::terminalYaw), Up));
}

void GameScene::Draw(int width, int height, const Camera* playerCamera) const
{
    if (width <= 0 || height <= 0)
    {
        return;
    }
    if (!campaignMode)
    {
        DrawBattle(width, height, playerCamera);
        return;
    }
    if (flow.Stage() == CampaignStage::Entry || flow.Screen() == CampaignScreen::Title ||
        flow.Screen() == CampaignScreen::Help)
    {
        DrawEntry(width, height);
    }
    else
    {
        DrawBattle(width, height, playerCamera);
    }
    if (!AnalysisActive())
    {
        ui.Draw(Page(), width, height, mouseX, mouseY);
    }
}

void GameScene::LoadStage()
{
    ++playerSpawnRevision;
    stageSmokeFrame = failedSmokeFrame = victorySmokeFrame = 0;
    combatScene.CloseAnalysis();
    combatScene.SetCorridorMode(true);
    combatScene.SetDroneMode(true);
    if (flow.Stage() == CampaignStage::Corridor)
    {
        combatScene.SetCombatMapMode(true);
    }
    if (flow.Stage() == CampaignStage::Maintenance)
    {
        combatScene.SetMaintenanceMode();
    }
    if (flow.Stage() == CampaignStage::Boss)
    {
        combatScene.SetCoreMode();
    }
    camera.SetPose(campaign_asset::playerStart, campaign_asset::playerYaw, campaign_asset::playerPitch);
    legacyTest = false;
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

void GameScene::ApplyAction(UiAction action)
{
    if (action != UiAction::Credits && action != UiAction::None)
    {
        showCredits = false;
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
        showCredits = true;
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
    if (!campaignMode)
    {
        return false;
    }
    if (showCredits)
    {
        showCredits = false;
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
    const auto add = [&](UiAction action, const wchar_t* label, int row)
    { page.buttons.push_back({action, label, 218.0f, 430.0f + 64 * row, 588, 52, true}); };
    switch (flow.Screen())
    {
    case CampaignScreen::Title:
        page.title = L"AEGIS  저항 작전";
        page.body = L"중앙 AI가 장악한 시설에 침투하세요.\n전투 복도를 돌파하고 코어를 파괴하는 것이 "
                    L"목표입니다.\n\n시작 → 안전 진입실 → 첫 복도 → 정비 복도 → 코어실";
        add(UiAction::Start, L"게임 시작", 0);
        add(UiAction::Help, L"조작 안내", 1);
        add(UiAction::Quit, L"게임 종료", 2);
        page.buttons.push_back({UiAction::Credits, L"제작·출처", 650, 155, 156, 42, true});
        page.footer = L"Enter: 게임 시작  ·  마우스로 버튼을 선택할 수 있습니다.";
        break;
    case CampaignScreen::Help:
        page.title = L"조작 안내";
        page.body = L"WASD 이동 · 마우스 조준 · 왼쪽 클릭 사격\n정지: 느린 시간 / 이동: 시간 진행\n사격 후 "
                    L"0.25초는 적도 정상 진행\n본체 명중 → 내 총 파손 / 빈손 + E: 재무장\n모든 적과 적탄 "
                    L"정리 → 다음 구역\nShift 정상 진행 · R 재시작 · Esc 메뉴\n보스: 코어를 공격하고 50% 이하 자기장을 피하세요.";
        add(UiAction::Title, L"시작 화면으로", 2);
        page.footer = L"G: 총 내려놓기 · E: 회수·보급·분석 · Enter: 안전 확보 후 다음 구역";
        break;
    case CampaignScreen::Paused:
        page.title = L"일시정지";
        page.body = L"전투와 세계 시간이 멈췄습니다.\n계속 진행하거나 현재 구역을 다시 시작할 수 있습니다.";
        for (const auto& b :
             std::vector<std::pair<UiAction, const wchar_t*>>{{UiAction::Resume, L"계속하기"},
                                                              {UiAction::Retry, L"현재 구역 다시 시작"},
                                                              {UiAction::Title, L"시작 화면으로"},
                                                              {UiAction::Quit, L"게임 종료"}})
        {
            page.buttons.push_back({b.first, b.second, 218,
                                    414.0f + 52 * static_cast<float>(page.buttons.size()), 588, 44, true});
        }
        page.footer = L"Esc 또는 Space: 계속하기";
        break;
    case CampaignScreen::Failed:
        page.title = L"임무 실패";
        if (flow.Stage() == CampaignStage::Maintenance)
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
        add(UiAction::Retry, L"현재 구역 다시 시작", 0);
        add(UiAction::Title, L"시작 화면으로", 1);
        add(UiAction::Quit, L"게임 종료", 2);
        page.footer = L"R 또는 Enter: 현재 구역 다시 시작";
        break;
    case CampaignScreen::Victory:
        page.title = L"작전 성공";
        page.body = L"중앙 AI의 코어를 파괴했습니다.\n시설 침투 작전을 완료했습니다.\n\n다시 시작하면 안전 "
                    L"진입실부터 진행합니다.";
        add(UiAction::Restart, L"처음부터 다시 시작", 0);
        add(UiAction::Title, L"시작 화면으로", 1);
        add(UiAction::Quit, L"게임 종료", 2);
        page.footer = L"R 또는 Enter: 처음부터 다시 시작";
        break;
    case CampaignScreen::Playing:
        page.panel = false;
        if (flow.Stage() == CampaignStage::Entry)
        {
            page.title = L"1 / 4  안전 진입실";
            page.body = L"이동: 시간 진행 / 적 본체 명중: 내 총 파손\n빈손으로 총 조준 + E: 재무장 / Enter: "
                        L"복도 진입";
            page.buttons.push_back({UiAction::Advance, L"복도로 진입", 744, 640, 232, 54, true});
        }
        else if (flow.Stage() == CampaignStage::Corridor)
        {
            page.title = L"2 / 4  새 전투 맵 · 첫 복도";
            page.body = ReadyToAdvance()
                            ? L"복도가 안전해졌습니다. 오른쪽 단말에서 분석하거나 정비 복도로 진행하세요."
                            : L"드론과 포탑을 제거하고 남은 적탄을 피하세요. 모든 위험이 사라지면 진행할 수 "
                              L"있습니다.";
            page.buttons.push_back(
                {UiAction::Advance, L"Enter · 정비 복도로", 744, 640, 232, 54, ReadyToAdvance()});
        }
        else if (flow.Stage() == CampaignStage::Maintenance)
        {
            page.title = L"3 / 4  정비 복도";
            page.body = L"코너의 드론을 파괴하고 남은 적탄을 정리하세요.";
            page.buttons.push_back(
                {UiAction::Advance, L"Enter · 코어실로 이동", 744, 640, 232, 54, ReadyToAdvance()});
        }
        else
        {
            page.title = L"4 / 4  코어실 보스전";
            page.body = L"호위 드론과 포탑을 피하며 중앙 코어를 파괴하세요. 코어 체력 50% 이하에서는 "
                        L"자기장이 발동합니다.\n빈손일 때 입구 보급대 조준 + E  /  Esc로 메뉴";
        }
        if (flow.Stage() != CampaignStage::Entry)
        {
            combatScene.PopulateHud(page);
        }
        break;
    }
    return page;
}

std::string GameScene::StatusTitle(bool paused) const
{
    if (!campaignMode)
    {
        return combatScene.StatusTitle(paused) + combatScene.TimeDebug();
    }
    const char* screen = showCredits                                ? "CREDITS"
                         : flow.Screen() == CampaignScreen::Title   ? "TITLE"
                         : flow.Screen() == CampaignScreen::Help    ? "HELP"
                         : flow.Screen() == CampaignScreen::Paused  ? "PAUSED"
                         : flow.Screen() == CampaignScreen::Failed  ? "FAILED"
                         : flow.Screen() == CampaignScreen::Victory ? "VICTORY"
                                                                    : "PLAYING";
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

void GameScene::SmokeFrame(int frame)
{
    if (playerViewTestMode)
    {
        combatScene.PlayerViewSmokeFrame(frame);
        return;
    }
    if (!campaignMode)
    {
        combatScene.SetSmokeFrame(frame);
        return;
    }
    if (frame == 20)
    {
        ApplyAction(UiAction::Help);
    }
    if (frame == 40)
    {
        ApplyAction(UiAction::Title);
    }
    if (frame == 60)
    {
        ApplyAction(UiAction::Start);
    }
    if (flow.Screen() == CampaignScreen::Playing)
    {
        ++stageSmokeFrame;
        if (flow.Stage() == CampaignStage::Entry)
        {
            if (smokeVictory && stageSmokeFrame > 20)
            {
                smokeRestarted = true;
                ApplyAction(UiAction::Title);
                smokeDone = true;
            }
            else if (!smokeVictory && stageSmokeFrame == 60)
            {
                ApplyAction(UiAction::Advance);
            }
        }
        else if (flow.Stage() == CampaignStage::Corridor)
        {
            if (!smokePaused && stageSmokeFrame == 5)
            {
                flow.TogglePause();
                smokePaused = true;
            }
            else if (smokeRetried)
            {
                combatScene.CampaignSmokeFrame(stageSmokeFrame);
                if (ReadyToAdvance())
                {
                    ApplyAction(UiAction::Advance);
                }
            }
        }
        else if (flow.Stage() == CampaignStage::Maintenance)
        {
            combatScene.CampaignSmokeFrame(stageSmokeFrame);
            if (ReadyToAdvance())
            {
                ApplyAction(UiAction::Advance);
            }
        }
        else
        {
            combatScene.CampaignSmokeFrame(stageSmokeFrame);
        }
    }
    else if (flow.Screen() == CampaignScreen::Paused)
    {
        if (++pausedSmokeFrame == 10)
        {
            ApplyAction(UiAction::Resume);
        }
    }
    else if (flow.Screen() == CampaignScreen::Failed && flow.Stage() == CampaignStage::Corridor &&
             !smokeRetried)
    {
        if (++failedSmokeFrame == 20)
        {
            smokeRetried = true;
            ApplyAction(UiAction::Retry);
        }
    }
    else if (flow.Screen() == CampaignScreen::Victory)
    {
        smokeVictory = combatScene.IsCoreMode() ? !combatScene.CoreState().CoreAlive()
                                               : combatScene.BossCoreHits() == 3;
        if (++victorySmokeFrame == 20)
        {
            ApplyAction(UiAction::Restart);
        }
    }
}

bool GameScene::SmokePassed() const
{
    if (playerViewTestMode)
    {
        return combatScene.PlayerViewSmokePassed();
    }
    return campaignMode ? smokeDone && smokeRetried && smokePaused && smokeVictory && smokeRestarted &&
                              flow.Screen() == CampaignScreen::Title
                        : combatScene.SmokePassed();
}

} // namespace aegis
