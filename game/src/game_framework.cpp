#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include "game_framework.h"

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include "game_scene.h"

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <string>

namespace aegis {
namespace {

struct FrameInput {
    GameScene* scene{};
    bool paused{};
    bool resetRequested{};
    bool restartFrameTime{};
    bool toggleModeRequested{};
};

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

void keyCallback(GLFWwindow* window, int key, int, int action, int)
{
    if (action != GLFW_PRESS) {
        return;
    }

    auto& input = *static_cast<FrameInput*>(glfwGetWindowUserPointer(window));
    if (key == GLFW_KEY_ESCAPE) {
        if (input.scene && input.scene->CloseAnalysis()) input.restartFrameTime=true;
        else if(input.scene && input.scene->IsCampaign()) { input.scene->TogglePause(); input.restartFrameTime=true; }
        else glfwSetWindowShouldClose(window, GLFW_TRUE);
    } else if (key == GLFW_KEY_SPACE) {
        if (input.scene && input.scene->AnalysisActive()) return;
        if (input.scene && input.scene->IsCampaign()) { input.scene->TogglePause(); input.restartFrameTime=true; return; }
        input.paused = !input.paused;
        input.restartFrameTime = true;
    } else if (key == GLFW_KEY_R) {
        input.resetRequested = true;
        input.restartFrameTime = true;
    } else if (key == GLFW_KEY_F2) {
        if(input.scene && input.scene->IsCampaign()) return;
        input.toggleModeRequested = true;
        input.restartFrameTime = true;
    }
}

void restartFrameTimeCallback(GLFWwindow* window, int)
{
    auto& input = *static_cast<FrameInput*>(glfwGetWindowUserPointer(window));
    input.restartFrameTime = true;
}

} // namespace

int GameFramework::Run(bool combatTest, bool smokeTest, bool turretTest, bool droneTest, bool bossTest, bool campaign,bool combatMapTest)
{
    const auto projectDirectory = ProjectDirectory();
    if (projectDirectory.empty()) {
        return 1;
    }

    glfwSetErrorCallback([](int, const char* message) {
        std::cerr << "GLFW: " << message << '\n';
    });
    if (!glfwInit()) {
        return 1;
    }

    glfwWindowHint(GLFW_VISIBLE, smokeTest ? GLFW_FALSE : GLFW_TRUE);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    GLFWwindow* window = glfwCreateWindow(1600, 1200, "AEGIS - OpenGL", nullptr, nullptr);
    if (!window) {
        glfwTerminate();
        return 1;
    }

    // 컨텍스트를 활성화한 뒤 GLEW로 OpenGL 함수를 불러온다.
    glfwMakeContextCurrent(window);
    glewExperimental = GL_TRUE;
    const GLenum glewStatus = glewInit();
    if (glewStatus != GLEW_OK) {
        std::cerr << "GLEW: " << glewGetErrorString(glewStatus) << '\n';
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }

    FrameInput input;
    GameScene scene;
    const bool ready = scene.Init(projectDirectory, combatTest, turretTest, droneTest, bossTest, campaign,combatMapTest);
    bool smokePassed = !smokeTest;
    if (ready) {
        input.scene=&scene;
        glfwSetWindowUserPointer(window, &input);
        glfwSetKeyCallback(window, keyCallback);
        glfwSetWindowFocusCallback(window, restartFrameTimeCallback);
        glfwSetWindowIconifyCallback(window, restartFrameTimeCallback);
        std::cout << "OpenGL " << glGetString(GL_VERSION) << '\n'
                  << (campaign ? "Campaign: mouse/Enter menus | LMB fire | RMB aim | B boss cover | E use | R checkpoint | Esc/Space menu\n" : "F2: corridor/range | F3: drone/turret | F4: boss | T: range/turret | LMB: fire | RMB drag: aim | B: cover | Space: pause | R: reset | Esc: exit\n")
                  << "F5: manual 1x/0.05x time preview | Shift: hold normal speed. Movement-based time is not connected yet." << std::endl;
        glfwSwapInterval(smokeTest ? 0 : 1);

        constexpr double maxDeltaTime = 0.05;
        double previousTime = glfwGetTime();
        bool wasActive = false;
        int smokeFrame = 0;
        bool savedEntry{},savedFailure{},savedVictory{},savedPause{},savedBossHud{},savedEmptyHud{},savedDamageEffect{},savedBreakEffect{};
        while (!glfwWindowShouldClose(window)) {
            glfwPollEvents();
            if (glfwWindowShouldClose(window)) {
                break;
            }

            const double currentTime = glfwGetTime();
            int width{}, height{};
            glfwGetFramebufferSize(window, &width, &height);
            const bool drawable = width > 0 && height > 0 &&
                                  !glfwGetWindowAttrib(window, GLFW_ICONIFIED);
            bool active = drawable && !input.paused &&
                                glfwGetWindowAttrib(window, GLFW_FOCUSED);
            if (input.toggleModeRequested) {
                scene.ToggleMode();
                input.toggleModeRequested = false;
                input.paused = false;
            }
            if (input.resetRequested) {
                scene.Reset();
                input.resetRequested = false;
            }

            scene.SampleInput(window, active && wasActive && !input.restartFrameTime && !smokeTest);
            if (scene.QuitRequested()) { glfwSetWindowShouldClose(window,GLFW_TRUE); break; }
            if (smokeTest) {
                active = true;
                scene.SmokeFrame(++smokeFrame);
                scene.Update(1.0 / 60.0,(1.0 / 60.0)*scene.PreviewTimeScale());
            }

            // 복귀한 첫 프레임에는 중단 중에 흐른 시간을 회전에 더하지 않는다.
            if (!smokeTest && active && wasActive && !input.restartFrameTime) {
                const double deltaTime = std::clamp(currentTime - previousTime, 0.0, maxDeltaTime);
                scene.Update(deltaTime,deltaTime*scene.PreviewTimeScale());
            }
            previousTime = currentTime;
            wasActive = active;
            input.restartFrameTime = false;

            if (drawable) {
                scene.Draw(width, height);
                if (smokeTest && scene.IsCampaign()) {
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
                if(smokeTest && combatMapTest && (smokeFrame==160 || smokeFrame==1360)) {
                    scene.TestScene().SaveFrame(projectDirectory/(smokeFrame==160 ? "build/combat-map-slow.ppm":"build/combat-map-clear.ppm"),width,height);
                }
                if (smokeTest && !scene.IsCampaign() && scene.TestScene().IsBossMode() && (smokeFrame==180 || smokeFrame==2300)) {
                    scene.TestScene().SaveFrame(projectDirectory/(smokeFrame==180 ? "build/boss-core-open-smoke.ppm":"build/boss-victory-smoke.ppm"),width,height);
                }
                if (smokeTest && !scene.IsCampaign() && scene.TestScene().IsDroneMode() && smokeFrame==1160) {
                    scene.TestScene().SaveFrame(projectDirectory/"build/analysis-terminal-smoke.ppm",width,height);
                }
                if (smokeTest && !scene.IsCampaign() && scene.TestScene().IsDroneMode() && (smokeFrame==1115 || smokeFrame==1250)) {
                    scene.TestScene().SaveFrame(projectDirectory/(smokeFrame==1115 ? "build/sector-securing-smoke.ppm" : "build/sector-open-smoke.ppm"),width,height);
                }
                if (smokeTest && !scene.IsCampaign() && scene.TestScene().IsDroneMode() && (smokeFrame==110 || smokeFrame==180)) {
                    scene.TestScene().SaveFrame(projectDirectory/(smokeFrame==110 ? "build/drone-warning-smoke.ppm" : "build/drone-shot-smoke.ppm"),width,height);
                }
                if (smokeTest && !scene.IsCampaign() && !scene.TestScene().IsTurretMode() && smokeFrame==125) {
                    scene.TestScene().SaveFrame(projectDirectory/"build/weapon-empty-smoke.ppm",width,height);
                }
                if (smokeTest && !scene.IsCampaign() && !scene.TestScene().IsTurretMode() && smokeFrame==5) {
                    if (!scene.TestScene().SaveFrame(projectDirectory/"build/weapon-fire-smoke.ppm",width,height))
                        std::cerr << "Could not save weapon recoil frame\n";
                }
                if (smokeTest && !scene.IsCampaign() && smokeFrame == 40) {
                    const auto capture=scene.TestScene().IsBossMode() ? "build/boss-warning-smoke.ppm":scene.TestScene().IsDroneMode() ? "build/drone-patrol-smoke.ppm" : scene.TestScene().IsCorridorMode() ? "build/corridor-combat-smoke.ppm" : scene.TestScene().IsTurretMode() ? "build/turret-warning-smoke.ppm" : "build/combat-range-smoke.ppm";
                    if (!scene.TestScene().SaveFrame(projectDirectory / capture, width, height)) {
                        std::cerr << "Could not save range smoke frame\n";
                    }
                }
                if (smokeTest && !scene.IsCampaign() && !scene.TestScene().IsBossMode() && scene.TestScene().IsTurretMode() && (smokeFrame==600 || smokeFrame==770)) {
                    const auto capture=smokeFrame==600 ? "build/turret-failed-smoke.ppm" : "build/turret-cleared-smoke.ppm";
                    if (!scene.TestScene().SaveFrame(projectDirectory / capture,width,height))
                        std::cerr << "Could not save turret result frame\n";
                }
                glfwSwapBuffers(window);
            } else {
                glfwWaitEventsTimeout(0.1);
            }
            if (scene.IsCombatTest()) {
                const std::string title=scene.StatusTitle(input.paused);
                glfwSetWindowTitle(window, title.c_str());
            } else {
                glfwSetWindowTitle(window, "AEGIS - Drone | F2: shooting range | Space: pause | R: reset");
            }
            if (smokeTest && smokeFrame >= scene.SmokeFrameCount()) {
                smokePassed = scene.SmokePassed();
                std::cout << (scene.IsCampaign() ? "Campaign smoke: ":combatMapTest ? "Combat map smoke: ":scene.TestScene().IsBossMode() ? "Boss combat smoke: ":scene.TestScene().IsDroneMode() ? "Drone combat smoke: " : scene.TestScene().IsCorridorMode() ? "Corridor combat smoke: " : scene.TestScene().IsTurretMode() ? "Turret encounter smoke: " : "Combat render smoke: ")
                    << (smokePassed ? "PASS" : "FAIL") << '\n';
                glfwSetWindowShouldClose(window, GLFW_TRUE);
            }
        }
    }

    // OpenGL 컨텍스트가 살아 있는 동안 장면이 가진 자원을 해제한다.
    scene.Clear();
    glfwDestroyWindow(window);
    glfwTerminate();
    return ready && smokePassed ? 0 : 1;
}

} // namespace aegis
