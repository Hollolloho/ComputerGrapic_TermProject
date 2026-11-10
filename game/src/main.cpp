#include "model_loader.h"

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <iostream>
#include <string_view>

namespace {

int InspectModel(const std::filesystem::path& path)
{
    aegis::ModelData model;
    std::string error;
    if (!aegis::LoadModel(path, model, error)) {
        std::cerr << error << '\n';
        return 1;
    }

    std::size_t vertices{};
    std::size_t triangles{};
    for (const auto& mesh : model.meshes) {
        vertices += mesh.vertices.size();
        triangles += mesh.indices.size() / 3;
    }
    std::cout << "meshes=" << model.meshes.size() << " vertices=" << vertices
              << " triangles=" << triangles << " materials=" << model.materials.size() << '\n';

    for (const auto& material : model.materials) {
        std::cout << material.name << " Kd=" << material.diffuse.r << ',' << material.diffuse.g
                  << ',' << material.diffuse.b << " opacity=" << material.opacity;
        if (!material.diffuseTexture.empty()) {
            std::cout << " map_Kd=" << material.diffuseTexture;
        }
        std::cout << '\n';
    }
    return 0;
}

int RunWindow()
{
    glfwSetErrorCallback([](int, const char* message) {
        std::cerr << "GLFW: " << message << '\n';
    });
    if (!glfwInit()) {
        return 1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    GLFWwindow* window = glfwCreateWindow(960, 540, "AEGIS - OpenGL", nullptr, nullptr);
    if (!window) {
        glfwTerminate();
        return 1;
    }

    glfwMakeContextCurrent(window);
    glewExperimental = GL_TRUE;
    const GLenum glewStatus = glewInit();
    if (glewStatus != GLEW_OK) {
        std::cerr << "GLEW: " << glewGetErrorString(glewStatus) << '\n';
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }

    std::cout << "OpenGL " << glGetString(GL_VERSION) << std::endl;
    glfwSwapInterval(1);
    glClearColor(0.08f, 0.11f, 0.15f, 1.0f);
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            glfwSetWindowShouldClose(window, GLFW_TRUE);
        }

        int width{}, height{};
        glfwGetFramebufferSize(window, &width, &height);
        if (width > 0 && height > 0) {
            glViewport(0, 0, width, height);
            glClear(GL_COLOR_BUFFER_BIT);
            glfwSwapBuffers(window);
        } else {
            glfwWaitEventsTimeout(0.1);
        }
    }

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}

} // namespace

int wmain(int argc, wchar_t* argv[])
{
    if (argc == 1) {
        return RunWindow();
    }
    if (argc == 3 && std::wstring_view(argv[1]) == L"--inspect-obj") {
        return InspectModel(argv[2]);
    }
    std::cerr << "Usage: AEGIS.exe [--inspect-obj <model.obj>]\n";
    return 2;
}
