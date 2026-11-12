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
    bool paused{};
    bool resetRequested{};
    bool restartFrameTime{};
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
        glfwSetWindowShouldClose(window, GLFW_TRUE);
    } else if (key == GLFW_KEY_SPACE) {
        input.paused = !input.paused;
        input.restartFrameTime = true;
    } else if (key == GLFW_KEY_R) {
        input.resetRequested = true;
        input.restartFrameTime = true;
    }
}

void restartFrameTimeCallback(GLFWwindow* window, int)
{
    auto& input = *static_cast<FrameInput*>(glfwGetWindowUserPointer(window));
    input.restartFrameTime = true;
}

} // namespace

int GameFramework::Run()
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
    const bool ready = scene.Init(projectDirectory);
    if (ready) {
        glfwSetWindowUserPointer(window, &input);
        glfwSetKeyCallback(window, keyCallback);
        glfwSetWindowFocusCallback(window, restartFrameTimeCallback);
        glfwSetWindowIconifyCallback(window, restartFrameTimeCallback);
        std::cout << "OpenGL " << glGetString(GL_VERSION) << '\n'
                  << "Space: pause/resume, R: reset pose, Esc: exit" << std::endl;
        glfwSwapInterval(1);

        constexpr double maxDeltaTime = 0.05;
        double previousTime = glfwGetTime();
        bool wasActive = false;
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
            const bool active = drawable && !input.paused &&
                                glfwGetWindowAttrib(window, GLFW_FOCUSED);
            if (input.resetRequested) {
                scene.Reset();
                input.resetRequested = false;
            }

            // 복귀한 첫 프레임에는 중단 중에 흐른 시간을 회전에 더하지 않는다.
            if (active && wasActive && !input.restartFrameTime) {
                const double deltaTime = std::clamp(currentTime - previousTime, 0.0, maxDeltaTime);
                scene.Update(deltaTime);
            }
            previousTime = currentTime;
            wasActive = active;
            input.restartFrameTime = false;

            if (drawable) {
                scene.Draw(width, height);
                glfwSwapBuffers(window);
            } else {
                glfwWaitEventsTimeout(0.1);
            }
        }
    }

    // OpenGL 컨텍스트가 살아 있는 동안 장면이 가진 자원을 해제한다.
    scene.Clear();
    glfwDestroyWindow(window);
    glfwTerminate();
    return ready ? 0 : 1;
}

} // namespace aegis
