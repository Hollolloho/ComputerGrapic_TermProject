#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include "FrameWork.h"
#include "game_framework.h"
#include <GLFW/glfw3.h>
#include <algorithm>
#include <iostream>

namespace {
std::filesystem::path ProjectDirectory()
{
    std::wstring executablePath(32768, L'\0');
    const DWORD length = GetModuleFileNameW(nullptr, executablePath.data(),
                                           static_cast<DWORD>(executablePath.size()));
    if (length == 0 || length >= executablePath.size()) {
        std::cerr << "Could not read the AEGIS executable path (Windows error "
                  << GetLastError() << ").\n";
        return {};
    }
    executablePath.resize(length);

    // build/<플랫폼>/<구성>/의 실행 파일 위치에서 프로젝트 루트로 올라간다.
    const auto outputDirectory = std::filesystem::path(executablePath).parent_path();
    return outputDirectory.parent_path().parent_path().parent_path();
}

} // namespace

void GameFrameWork::KeyCallback(GLFWwindow* window,int key,int,int action,int) {
    if(action!=GLFW_PRESS) return;
    auto& game=*static_cast<GameFrameWork*>(glfwGetWindowUserPointer(window));
    if(key==GLFW_KEY_ESCAPE) {
        if(game.scene.CloseAnalysis()) game.restartFrameTime=true;
        else if(game.scene.IsCampaign()) { game.scene.TogglePause(); game.restartFrameTime=true; }
        else glfwSetWindowShouldClose(window,GLFW_TRUE);
    } else if(key==GLFW_KEY_SPACE) {
        if(game.scene.AnalysisActive()) return;
        if(game.scene.IsCampaign()) { game.scene.TogglePause(); game.restartFrameTime=true; return; }
        game.paused=!game.paused; game.restartFrameTime=true;
    } else if(key==GLFW_KEY_R) {
        game.resetRequested=true; game.restartFrameTime=true;
    } else if(key==GLFW_KEY_F2 && !game.scene.IsCampaign()) {
        game.toggleModeRequested=true; game.restartFrameTime=true;
    }
}
void GameFrameWork::RestartFrameTimeCallback(GLFWwindow* window,int) {
    static_cast<GameFrameWork*>(glfwGetWindowUserPointer(window))->restartFrameTime=true;
}

int GameFrameWork::Run(const GameRunOptions& runOptions) {
    options=runOptions;
    if(!Init()) { Clear(); return 1; }
    bool smokePassed=!options.smokeTest;
    double previousTime=glfwGetTime();
    while(!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        if(glfwWindowShouldClose(window)) break;
        const double currentTime=glfwGetTime();
        const float deltaTime=static_cast<float>(std::clamp(currentTime-previousTime,0.0,0.05));
        previousTime=currentTime;
        Update(deltaTime);
        if(glfwWindowShouldClose(window)) break;
        Draw();
        if(options.smokeTest && smokeFrame>=scene.SmokeFrameCount()) {
            smokePassed=scene.SmokePassed();
            const char* label=options.playerViewTest ? "External player view smoke: "
                : scene.IsCampaign() ? "Campaign smoke: " : options.combatMapTest ? "Combat map smoke: "
                : scene.TestScene().IsBossMode() ? "Boss combat smoke: " : scene.TestScene().IsDroneMode() ? "Drone combat smoke: "
                : scene.TestScene().IsCorridorMode() ? "Corridor combat smoke: " : scene.TestScene().IsTurretMode() ? "Turret encounter smoke: " : "Combat render smoke: ";
            std::cout<<label<<(smokePassed ? "PASS":"FAIL")<<'\n';
            break;
        }
    }
    Clear();
    return smokePassed ? 0:1;
}

bool GameFrameWork::Init() {
    projectDirectory=ProjectDirectory();
    if(projectDirectory.empty()) return false;
    glfwSetErrorCallback([](int,const char* message) { std::cerr<<"GLFW: "<<message<<'\n'; });
    if(!glfwInit()) return false;
    glfwReady=true;
    glfwWindowHint(GLFW_VISIBLE,options.smokeTest ? GLFW_FALSE:GLFW_TRUE);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,3);
    glfwWindowHint(GLFW_OPENGL_PROFILE,GLFW_OPENGL_CORE_PROFILE);
    window=glfwCreateWindow(1600,1200,"AEGIS - OpenGL",nullptr,nullptr);
    if(!window) return false;
    glfwMakeContextCurrent(window);
    glewExperimental=GL_TRUE;
    const auto status=glewInit();
    if(status!=GLEW_OK) { std::cerr<<"GLEW: "<<glewGetErrorString(status)<<'\n'; return false; }
    sceneAttempted=true;
    if(!scene.Init(projectDirectory,options.combatTest,options.turretTest,options.droneTest,options.bossTest,
        options.campaign,options.combatMapTest,options.playerViewTest)) return false;
    glfwSetWindowUserPointer(window,this);
    glfwSetKeyCallback(window,KeyCallback);
    glfwSetWindowFocusCallback(window,RestartFrameTimeCallback);
    glfwSetWindowIconifyCallback(window,RestartFrameTimeCallback);
    glfwSwapInterval(options.smokeTest ? 0:1);
    std::cout<<"OpenGL "<<glGetString(GL_VERSION)<<'\n'
        <<(options.campaign ? "Campaign: mouse/Enter menus | LMB fire | RMB aim | B boss cover | E use | R checkpoint | Esc/Space menu\n"
            :"F2: corridor/range | F3: drone/turret | F4: boss | T: range/turret | LMB: fire | RMB drag: aim | B: cover | Space: pause | R: reset | Esc: exit\n")
        <<"F5: manual 1x/0.05x time preview | Shift: hold normal speed. Movement-based time is not connected yet."<<std::endl;
    return true;
}

void GameFrameWork::Update(float deltaTime) {
    glfwGetFramebufferSize(window,&width,&height);
    drawable=width>0 && height>0 && !glfwGetWindowAttrib(window,GLFW_ICONIFIED);
    bool active=drawable && !paused && glfwGetWindowAttrib(window,GLFW_FOCUSED);
    if(toggleModeRequested) { scene.ToggleMode(); toggleModeRequested=false; paused=false; }
    if(resetRequested) { scene.Reset(); resetRequested=false; }
    scene.SampleInput(window,active && wasActive && !restartFrameTime && !options.smokeTest);
    if(scene.QuitRequested()) { glfwSetWindowShouldClose(window,GLFW_TRUE); return; }
    if(options.smokeTest) {
        active=true; scene.SmokeFrame(++smokeFrame);
        scene.Update(1.0/60,(1.0/60)*scene.PreviewTimeScale());
    } else if(active && wasActive && !restartFrameTime) {
        scene.Update(deltaTime,deltaTime*scene.PreviewTimeScale());
    }
    wasActive=active; restartFrameTime=false;
}

void GameFrameWork::Draw() {
    if(drawable) {
        scene.Draw(width,height);
                if (options.smokeTest && scene.IsCampaign()) {
                    const auto title=scene.StatusTitle(false);
                    const auto save=[&](const char* name) { scene.TestScene().SaveFrame(projectDirectory/name,width,height); };
                    if(smokeFrame==10) save("build/campaign-title-smoke.ppm");
                    if(smokeFrame==30) save("build/campaign-help-smoke.ppm");
                    if(smokeFrame==160) save("build/campaign-corridor-hud.ppm");
                    const auto& feedback=scene.TestScene().Feedback();
                    if(!savedDamageEffect && feedback.DamageStrength()>0.5f) { save("build/campaign-damage-effect.ppm"); savedDamageEffect=true; }
                    if(!savedBreakEffect && !feedback.Shards().empty()) { save("build/campaign-weapon-break.ppm"); savedBreakEffect=true; }

                    if(!savedBossHud && title.find("PLAYING | BOSS")!=std::string::npos) { save("build/campaign-boss-hud.ppm"); savedBossHud=true; }
                    if(!savedEmptyHud && title.find("PLAYING | BOSS")!=std::string::npos && scene.TestScene().BossCoreHits()==1) { save("build/campaign-empty-hud.ppm"); savedEmptyHud=true; }

                    if(!savedEntry && title.find("PLAYING | ENTRY")!=std::string::npos) { save("build/campaign-entry-smoke.ppm"); savedEntry=true; }
                    if(!savedFailure && title.find("CAMPAIGN | FAILED")!=std::string::npos) { save("build/campaign-failed-smoke.ppm"); savedFailure=true; }
                    if(!savedPause && title.find("CAMPAIGN | PAUSED")!=std::string::npos) { save("build/campaign-paused-smoke.ppm"); savedPause=true; }
                    if(!savedVictory && title.find("CAMPAIGN | VICTORY")!=std::string::npos) { save("build/campaign-victory-smoke.ppm"); savedVictory=true; }
                }
                if(options.smokeTest && options.playerViewTest && (smokeFrame==20 || smokeFrame==110)) {
                    scene.TestScene().SaveFrame(projectDirectory/(smokeFrame==20 ? "build/external-player-view.ppm":"build/external-player-analysis.ppm"),width,height);
                }
                if(options.smokeTest && options.combatMapTest && (smokeFrame==160 || smokeFrame==1360)) {
                    scene.TestScene().SaveFrame(projectDirectory/(smokeFrame==160 ? "build/combat-map-slow.ppm":"build/combat-map-clear.ppm"),width,height);
                }
                if (options.smokeTest && !scene.IsCampaign() && scene.TestScene().IsBossMode() && (smokeFrame==180 || smokeFrame==2300)) {
                    scene.TestScene().SaveFrame(projectDirectory/(smokeFrame==180 ? "build/boss-core-open-smoke.ppm":"build/boss-victory-smoke.ppm"),width,height);
                }
                if (options.smokeTest && !scene.IsCampaign() && scene.TestScene().IsDroneMode() && smokeFrame==1160) {
                    scene.TestScene().SaveFrame(projectDirectory/"build/analysis-terminal-smoke.ppm",width,height);
                }
                if (options.smokeTest && !scene.IsCampaign() && scene.TestScene().IsDroneMode() && (smokeFrame==1115 || smokeFrame==1250)) {
                    scene.TestScene().SaveFrame(projectDirectory/(smokeFrame==1115 ? "build/sector-securing-smoke.ppm" : "build/sector-open-smoke.ppm"),width,height);
                }
                if (options.smokeTest && !scene.IsCampaign() && scene.TestScene().IsDroneMode() && (smokeFrame==110 || smokeFrame==180)) {
                    scene.TestScene().SaveFrame(projectDirectory/(smokeFrame==110 ? "build/drone-warning-smoke.ppm" : "build/drone-shot-smoke.ppm"),width,height);
                }
                if (options.smokeTest && !scene.IsCampaign() && !scene.TestScene().IsTurretMode() && smokeFrame==125) {
                    scene.TestScene().SaveFrame(projectDirectory/"build/weapon-empty-smoke.ppm",width,height);
                }
                if (options.smokeTest && !scene.IsCampaign() && !scene.TestScene().IsTurretMode() && smokeFrame==5) {
                    if (!scene.TestScene().SaveFrame(projectDirectory/"build/weapon-fire-smoke.ppm",width,height))
                        std::cerr << "Could not save weapon recoil frame\n";
                }
                if (options.smokeTest && !scene.IsCampaign() && smokeFrame == 40) {
                    const auto capture=scene.TestScene().IsBossMode() ? "build/boss-warning-smoke.ppm":scene.TestScene().IsDroneMode() ? "build/drone-patrol-smoke.ppm" : scene.TestScene().IsCorridorMode() ? "build/corridor-combat-smoke.ppm" : scene.TestScene().IsTurretMode() ? "build/turret-warning-smoke.ppm" : "build/combat-range-smoke.ppm";
                    if (!scene.TestScene().SaveFrame(projectDirectory / capture, width, height)) {
                        std::cerr << "Could not save range smoke frame\n";
                    }
                }
                if (options.smokeTest && !scene.IsCampaign() && !scene.TestScene().IsBossMode() && scene.TestScene().IsTurretMode() && (smokeFrame==600 || smokeFrame==770)) {
                    const auto capture=smokeFrame==600 ? "build/turret-failed-smoke.ppm" : "build/turret-cleared-smoke.ppm";
                    if (!scene.TestScene().SaveFrame(projectDirectory / capture,width,height))
                        std::cerr << "Could not save turret result frame\n";
                }
        glfwSwapBuffers(window);
    } else glfwWaitEventsTimeout(0.1);
    const auto title=scene.StatusTitle(paused);
    glfwSetWindowTitle(window,title.c_str());
}

void GameFrameWork::Clear() {
    // Release OpenGL resources while their context still exists, including partial Init failures.
    if(sceneAttempted) { scene.Clear(); sceneAttempted=false; }
    if(window) { glfwDestroyWindow(window); window=nullptr; }
    if(glfwReady) { glfwTerminate(); glfwReady=false; }
}

// Compatibility with callers of the previous combat framework; only one loop owns GLFW.
int aegis::GameFramework::Run(bool combatTest,bool smokeTest,bool turretTest,bool droneTest,
    bool bossTest,bool campaign,bool combatMapTest,bool playerViewTest) {
    GameFrameWork game;
    return game.Run({combatTest,smokeTest,turretTest,droneTest,bossTest,campaign,combatMapTest,playerViewTest});
}
