#include "game_scene.h"
#include "corridor_layout.generated.h"
#include "campaign_layout.generated.h"
#include "terminal_layout.generated.h"
#include <GLFW/glfw3.h>

#include <GL/glew.h>
#include <glm/gtc/matrix_transform.hpp>

#include <cmath>
#include <fstream>
#include <iostream>

namespace aegis {
namespace {

using corridor_asset::placements;

const glm::vec3 Up(0.0f, 1.0f, 0.0f);

} // namespace

bool GameScene::Init(const std::filesystem::path& projectDirectory, bool useCombatTest, bool turretTest, bool droneTest, bool bossTest, bool campaign)
{
    Clear();
    legacyTest=useCombatTest; campaignMode=campaign;
    glEnable(GL_DEPTH_TEST);
    // 메시 셰이더가 최종 색을 sRGB로 변환한다.
    glDisable(GL_FRAMEBUFFER_SRGB);
    glClearColor(0.16f, 0.19f, 0.24f, 1.0f);
    if (!renderer.Init(projectDirectory / "game" / "shaders") || !InitCorridor(projectDirectory)
        || !testScene.Init(projectDirectory) || !InitEntry(projectDirectory) || !ui.Init()) return false;
    testScene.SetCorridorMode(!legacyTest);
    if (legacyTest) testScene.SetTurretMode(turretTest);
    else testScene.SetDroneMode(droneTest);
    if (bossTest) testScene.SetBossMode(true);
    testScene.SetDebugControls(!campaignMode);
    flow.ToTitle();
    if (campaignMode) LoadStage();
    return true;
}

bool GameScene::InitCorridor(const std::filesystem::path& projectDirectory)
{
    const auto directory = projectDirectory / "asset" / "corridor" / "test_scene";
    const auto materialPath = directory / "materials.mtl";
    if (!std::ifstream(materialPath)) {
        std::cerr << "Could not read corridor materials: " << materialPath << '\n';
        return false;
    }
    const ObjModelCache cache(projectDirectory / "build" / "cache" / "models", materialPath);

    static_assert(std::size(placements) == std::tuple_size_v<decltype(corridor)>);
    for (std::size_t i = 0; i < corridor.size(); ++i) {
        auto& object = corridor[i];
        const auto& placement = placements[i];
        const glm::mat4 parent = placement.parent>=0 ? corridor[placement.parent].model : glm::mat4(1.0f);
        object.model = glm::rotate(glm::translate(parent, placement.translation),
                                   glm::radians(placement.yawDegrees), Up);

        const auto path = directory / placement.file;
        ModelData model;
        std::string error;
        bool cacheHit{};
        if (!cache.Load(path, model, error, cacheHit)) {
            std::cerr << error << '\n';
            return false;
        }

        for (const auto& mesh : model.meshes) {
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

void GameScene::Update(double deltaTime)
{
    if (campaignMode && !flow.WorldRuns()) return;
    testScene.Update(deltaTime,deltaTime);
    if (campaignMode) flow.ObserveBattle(testScene.BattleResult()==combat::EncounterResult::Failed,
        testScene.BattleResult()==combat::EncounterResult::Cleared);
}

void GameScene::DrawBattle(int width, int height) const
{
    const bool showHud=!campaignMode;
    if (legacyTest) { testScene.Draw(width,height,{},showHud); return; }
    testScene.Draw(width,height,[this](const glm::mat4& vp,const glm::vec3& eye) {
        renderer.Begin(eye,{0.45f,0.9f,0.7f},{1,1,1},2.5f,0.45f);
        for (std::size_t i=0;i<corridor.size();++i) {
            auto model=corridor[i].model;
            if (i==2 || i==3) model=glm::translate(model,glm::vec3((i==2 ? -1:1)*corridor_asset::doorTravel*testScene.DoorProgress(),0,0));
            renderer.Draw(corridor[i].meshes,vp,model);
        }
    },showHud);
}

void GameScene::Reset()
{
    if (campaignMode) {
        const bool reset=flow.Screen()==CampaignScreen::Victory ? flow.Restart():flow.Retry();
        if (reset) LoadStage();
        return;
    }
    drone.Reset();
    camera.Reset();
    testScene.Reset();
}

void GameScene::Clear()
{
    drone.Clear();
    testScene.Clear();
    for (auto& object : corridor) {
        for (auto& mesh : object.meshes) {
            mesh.Clear();
        }
        object.meshes.clear();
    }
    for(auto* meshes:{&entryRoom,&entryTerminal}) { for(auto& mesh:*meshes) mesh.Clear(); meshes->clear(); }
    ui.Clear(); renderer.Clear();
}

void GameScene::ToggleMode() {
    if (campaignMode) return;
    legacyTest=!legacyTest;
    testScene.SetCorridorMode(!legacyTest);
    if (legacyTest) testScene.SetTurretMode(false);
}
void GameScene::SampleInput(GLFWwindow* window,bool active) {
    if (!campaignMode) { testScene.SampleInput(window,active); return; }
    double x{},y{}; int width{},height{}; glfwGetCursorPos(window,&x,&y); glfwGetWindowSize(window,&width,&height);
    mouseX=width>0 ? static_cast<float>(x)*1024/width:0; mouseY=height>0 ? static_cast<float>(y)*768/height:0;
    const bool left=glfwGetMouseButton(window,GLFW_MOUSE_BUTTON_LEFT)==GLFW_PRESS;
    const bool enter=glfwGetKey(window,GLFW_KEY_ENTER)==GLFW_PRESS;
    bool consumed=false;
    if (active && !AnalysisActive()) {
        const auto page=Page(); const auto action=KoreanUi::Hit(page,mouseX,mouseY,consumed);
        const bool overUi=consumed; consumed=left && overUi;
        if (left && !mouseHeld && overUi) ApplyAction(action);
        else if (enter && !enterHeld && !page.buttons.empty()) {
            consumed=true; if(page.buttons[0].enabled) ApplyAction(page.buttons[0].action);
        }
    }
    const bool battle=flow.WorldRuns() && !consumed;
    testScene.SampleInput(window,active && battle);
    mouseHeld=left; enterHeld=enter;
}


bool GameScene::InitEntry(const std::filesystem::path& root) {
    const auto load=[&](const std::filesystem::path& path,std::vector<DrawMesh>& meshes) {
        ModelData model; std::string error;
        if (!LoadModel(path,model,error)) { std::cerr << error << '\n'; return false; }
        for(const auto& mesh:model.meshes) { auto& draw=meshes.emplace_back(); draw.Upload(mesh,model.materials.at(mesh.materialIndex)); }
        return true;
    };
    return load(root/campaign_asset::entryMesh,entryRoom) && load(root/terminal_asset::mesh,entryTerminal);
}
void GameScene::DrawEntry(int width,int height) const {
    glViewport(0,0,width,height); glDisable(GL_SCISSOR_TEST); glDisable(GL_FRAMEBUFFER_SRGB); glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST); glDepthMask(GL_TRUE); glDisable(GL_CULL_FACE); glClearColor(0.03f,0.06f,0.09f,1);
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    const auto vp=glm::perspective(glm::radians(60.0f),static_cast<float>(width)/height,0.05f,80.0f)*camera.ViewMatrix();
    renderer.Begin(camera.EyePosition(),{0.45f,0.9f,0.7f},{1,1,1},3,0.35f);
    renderer.Draw(entryRoom,vp,glm::mat4(1));
    for (const auto& object:corridor) renderer.Draw(object.meshes,vp,object.model);
    renderer.Draw(entryTerminal,vp,glm::rotate(glm::translate(glm::mat4(1),campaign_asset::terminalPosition),glm::radians(campaign_asset::terminalYaw),Up));
}
void GameScene::Draw(int width,int height) const {
    if (width<=0 || height<=0) return;
    if (!campaignMode) { DrawBattle(width,height); return; }
    if (flow.Stage()==CampaignStage::Entry || flow.Screen()==CampaignScreen::Title || flow.Screen()==CampaignScreen::Help) DrawEntry(width,height);
    else DrawBattle(width,height);
    if (!AnalysisActive()) ui.Draw(Page(),width,height,mouseX,mouseY);
}
void GameScene::LoadStage() {
    stageSmokeFrame=failedSmokeFrame=victorySmokeFrame=0;
    testScene.CloseAnalysis();
    testScene.SetCorridorMode(true); testScene.SetDroneMode(true);
    if(flow.Stage()==CampaignStage::Boss) testScene.SetBossMode(true);
    camera.SetPose(campaign_asset::playerStart,campaign_asset::playerYaw,campaign_asset::playerPitch);
    legacyTest=false;
}
bool GameScene::ReadyToAdvance() const {
    return flow.Screen()==CampaignScreen::Playing && !AnalysisActive()
        && (flow.Stage()==CampaignStage::Entry || (flow.Stage()==CampaignStage::Corridor
            && testScene.CanAnalyze() && testScene.DoorProgress()>=1));
}
void GameScene::ApplyAction(UiAction action) {
    switch(action) {
    case UiAction::Start: if(flow.Start()) LoadStage(); break;
    case UiAction::Help: flow.ShowHelp(); break;
    case UiAction::Title: flow.ToTitle(); LoadStage(); break;
    case UiAction::Advance: if(ReadyToAdvance() && flow.Advance(true)) LoadStage(); break;
    case UiAction::Resume: if(flow.Screen()==CampaignScreen::Paused) flow.TogglePause(); break;
    case UiAction::Retry: if(flow.Retry()) LoadStage(); break;
    case UiAction::Restart: if(flow.Restart()) LoadStage(); break;
    case UiAction::Quit: quitRequested=true; break;
    case UiAction::None: break;
    }
}
bool GameScene::TogglePause() {
    if (!campaignMode) return false;
    if (flow.Screen()==CampaignScreen::Help) { ApplyAction(UiAction::Title); return true; }
    return flow.TogglePause();
}
UiPage GameScene::Page() const {
    UiPage page;
    const auto add=[&](UiAction action,const wchar_t* label,int row) { page.buttons.push_back({action,label,218.0f,430.0f+64*row,588,52,true}); };
    switch(flow.Screen()) {
    case CampaignScreen::Title:
        page.title=L"AEGIS  저항 작전";
        page.body=L"중앙 AI가 장악한 시설에 침투하세요.\n전투 복도를 돌파하고 코어를 파괴하는 것이 목표입니다.\n\n시작 → 안전 진입실 → 전투 복도 → 코어실";
        add(UiAction::Start,L"게임 시작",0); add(UiAction::Help,L"조작 안내",1); add(UiAction::Quit,L"게임 종료",2);
        page.footer=L"Enter: 게임 시작  ·  마우스로 버튼을 선택할 수 있습니다."; break;
    case CampaignScreen::Help:
        page.title=L"조작 안내";
        page.body=L"왼쪽 클릭: 한 발 사격  /  오른쪽 드래그: 조준\nE: 무기 회수·보급·안전한 분석 단말 사용\nB: 보스전 엄폐·사격 위치 전환\nR: 현재 구역 다시 시작\nEsc 또는 Space: 일시정지 메뉴\n열린 보스 코어를 세 번 맞히면 승리합니다.";
        add(UiAction::Title,L"시작 화면으로",2);
        page.footer=L"현재 구역 이동은 진입 버튼으로 진행합니다."; break;
    case CampaignScreen::Paused:
        page.title=L"일시정지"; page.body=L"전투와 세계 시간이 멈췄습니다.\n계속 진행하거나 현재 구역을 다시 시작할 수 있습니다.";
        for(const auto& b:std::vector<std::pair<UiAction,const wchar_t*>>{{UiAction::Resume,L"계속하기"},{UiAction::Retry,L"현재 구역 다시 시작"},{UiAction::Title,L"시작 화면으로"},{UiAction::Quit,L"게임 종료"}}) {
            page.buttons.push_back({b.first,b.second,218,414.0f+52*static_cast<float>(page.buttons.size()),588,44,true});
        }
        page.footer=L"Esc 또는 Space: 계속하기"; break;
    case CampaignScreen::Failed:
        page.title=L"임무 실패";
        page.body=flow.Stage()==CampaignStage::Boss ? L"코어실에서 전투 불능이 되었습니다.\n다시 시도하면 보스전부터 시작합니다.":L"전투 복도에서 전투 불능이 되었습니다.\n다시 시도하면 복도 전투부터 시작합니다.";
        add(UiAction::Retry,L"현재 구역 다시 시작",0); add(UiAction::Title,L"시작 화면으로",1); add(UiAction::Quit,L"게임 종료",2);
        page.footer=L"R 또는 Enter: 현재 구역 다시 시작"; break;
    case CampaignScreen::Victory:
        page.title=L"작전 성공"; page.body=L"중앙 AI의 코어를 파괴했습니다.\n시설 침투 작전을 완료했습니다.\n\n다시 시작하면 안전 진입실부터 진행합니다.";
        add(UiAction::Restart,L"처음부터 다시 시작",0); add(UiAction::Title,L"시작 화면으로",1); add(UiAction::Quit,L"게임 종료",2);
        page.footer=L"R 또는 Enter: 처음부터 다시 시작"; break;
    case CampaignScreen::Playing:
        page.panel=false;
        if(flow.Stage()==CampaignStage::Entry) {
            page.title=L"1 / 3  안전 진입실";
            page.body=L"시설 진입을 준비합니다. 조준은 오른쪽 드래그, 사격은 왼쪽 클릭입니다.\n버튼 또는 Enter로 전투 복도에 진입하세요.";
            page.buttons.push_back({UiAction::Advance,L"복도로 진입",744,640,232,54,true});
        } else if(flow.Stage()==CampaignStage::Corridor) {
            page.title=L"2 / 3  전투 복도";
            page.body=ReadyToAdvance() ? L"복도가 안전해졌습니다. 오른쪽 단말에서 분석하거나 코어실로 진행하세요.":L"드론을 제거하고 남은 적탄을 피하세요. 모든 위험이 사라지면 문이 열립니다.";
            page.buttons.push_back({UiAction::Advance,L"코어실로 이동",744,640,232,54,ReadyToAdvance()});
        } else {
            page.title=L"3 / 3  코어실 보스전";
            page.body=L"장갑이 완전히 열렸을 때만 코어를 공격하세요. 세 번의 유효타가 필요합니다.\nB로 엄폐·사격 위치 전환  /  빈손일 때 E로 보급  /  Esc로 메뉴";
        }
        if(flow.Stage()!=CampaignStage::Entry) testScene.PopulateHud(page);
        break;
    }
    return page;
}
std::string GameScene::StatusTitle(bool paused) const {
    if(!campaignMode) return testScene.StatusTitle(paused);
    const char* screen=flow.Screen()==CampaignScreen::Title?"TITLE":flow.Screen()==CampaignScreen::Help?"HELP":flow.Screen()==CampaignScreen::Paused?"PAUSED":flow.Screen()==CampaignScreen::Failed?"FAILED":flow.Screen()==CampaignScreen::Victory?"VICTORY":"PLAYING";
    const char* stage=flow.Stage()==CampaignStage::Entry?"ENTRY":flow.Stage()==CampaignStage::Corridor?"CORRIDOR":"BOSS";
    return std::string("AEGIS CAMPAIGN | ")+screen+" | "+stage+" | "+testScene.StatusTitle(false);
}
void GameScene::SmokeFrame(int frame) {
    if(!campaignMode) { testScene.SetSmokeFrame(frame); return; }
    if(frame==20) ApplyAction(UiAction::Help);
    if(frame==40) ApplyAction(UiAction::Title);
    if(frame==60) ApplyAction(UiAction::Start);
    if(flow.Screen()==CampaignScreen::Playing) {
        ++stageSmokeFrame;
        if(flow.Stage()==CampaignStage::Entry) {
            if(smokeVictory && stageSmokeFrame>20) { smokeRestarted=true; ApplyAction(UiAction::Title); smokeDone=true; }
            else if(!smokeVictory && stageSmokeFrame==60) ApplyAction(UiAction::Advance);
        } else if(flow.Stage()==CampaignStage::Corridor) {
            if(!smokePaused && stageSmokeFrame==5) { flow.TogglePause(); smokePaused=true; }
            else if(smokeRetried) { testScene.CampaignSmokeFrame(stageSmokeFrame); if(ReadyToAdvance()) ApplyAction(UiAction::Advance); }
        } else testScene.CampaignSmokeFrame(stageSmokeFrame);
    } else if(flow.Screen()==CampaignScreen::Paused) {
        if(++pausedSmokeFrame==10) ApplyAction(UiAction::Resume);
    } else if(flow.Screen()==CampaignScreen::Failed && flow.Stage()==CampaignStage::Corridor && !smokeRetried) {
        if(++failedSmokeFrame==20) { smokeRetried=true; ApplyAction(UiAction::Retry); }
    } else if(flow.Screen()==CampaignScreen::Victory) {
        smokeVictory=testScene.BossCoreHits()==3;
        if(++victorySmokeFrame==20) ApplyAction(UiAction::Restart);
    }
}
bool GameScene::SmokePassed() const {
    return campaignMode ? smokeDone && smokeRetried && smokePaused && smokeVictory && smokeRestarted
        && flow.Screen()==CampaignScreen::Title:testScene.SmokePassed();
}

} // namespace aegis
