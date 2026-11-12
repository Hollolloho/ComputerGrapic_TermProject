#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include "model_loader.h"

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <array>
#include <cstddef>
#include <fstream>
#include <initializer_list>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace {

struct DrawMesh {
    GLuint vao{};
    GLuint vbo{};
    GLuint ebo{};
    GLsizei indexCount{};
    glm::vec3 color{1.0f};
};

struct DronePart {
    const char* name;
    glm::vec3 origin;
    std::initializer_list<const char*> materials;
    std::vector<DrawMesh> meshes;
};

using Drone = std::array<DronePart, 5>;

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

GLuint CompileShader(GLenum shaderType, const std::filesystem::path& path)
{
    std::ifstream file(path);
    std::ostringstream sourceStream;
    if (file) {
        sourceStream << file.rdbuf();
    }
    if (!file || !sourceStream) {
        std::cerr << "Could not read shader: " << path << '\n';
        return 0;
    }

    const std::string source = sourceStream.str();
    const char* code = source.c_str();
    // 파일에서 읽은 GLSL 코드를 OpenGL에서 컴파일한다.
    const GLuint shader = glCreateShader(shaderType);
    glShaderSource(shader, 1, &code, nullptr);
    glCompileShader(shader);

    GLint compiled{};
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (!compiled) {
        GLint logLength{};
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);
        std::string log(logLength, '\0');
        glGetShaderInfoLog(shader, logLength, nullptr, log.data());
        std::cerr << "Shader compile failed: " << path << '\n' << log.c_str() << '\n';
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

GLuint MakeShaderProgram(const std::filesystem::path& shaderDirectory)
{
    const auto vertexPath = shaderDirectory / "drone.vert";
    const auto fragmentPath = shaderDirectory / "drone.frag";
    const GLuint vertexShader = CompileShader(GL_VERTEX_SHADER, vertexPath);
    if (!vertexShader) {
        return 0;
    }
    const GLuint fragmentShader = CompileShader(GL_FRAGMENT_SHADER, fragmentPath);
    if (!fragmentShader) {
        glDeleteShader(vertexShader);
        return 0;
    }

    const GLuint shaderProgramID = glCreateProgram();
    glAttachShader(shaderProgramID, vertexShader);
    glAttachShader(shaderProgramID, fragmentShader);
    glLinkProgram(shaderProgramID);
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    GLint linked{};
    glGetProgramiv(shaderProgramID, GL_LINK_STATUS, &linked);
    if (!linked) {
        GLint logLength{};
        glGetProgramiv(shaderProgramID, GL_INFO_LOG_LENGTH, &logLength);
        std::string log(logLength, '\0');
        glGetProgramInfoLog(shaderProgramID, logLength, nullptr, log.data());
        std::cerr << "Shader link failed: " << vertexPath << " and " << fragmentPath
                  << '\n' << log.c_str() << '\n';
        glDeleteProgram(shaderProgramID);
        return 0;
    }
    return shaderProgramID;
}

void InitVertexBuffer(DrawMesh& drawMesh, const aegis::ModelMesh& mesh)
{
    // VAO에 정점 속성과 인덱스 버퍼 연결을 기록한다.
    glGenVertexArrays(1, &drawMesh.vao);
    glGenBuffers(1, &drawMesh.vbo);
    glGenBuffers(1, &drawMesh.ebo);
    glBindVertexArray(drawMesh.vao);

    glBindBuffer(GL_ARRAY_BUFFER, drawMesh.vbo);
    glBufferData(GL_ARRAY_BUFFER, mesh.vertices.size() * sizeof(aegis::ModelVertex),
                 mesh.vertices.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, drawMesh.ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, mesh.indices.size() * sizeof(unsigned int),
                 mesh.indices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(aegis::ModelVertex),
                          reinterpret_cast<const void*>(offsetof(aegis::ModelVertex, position)));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(aegis::ModelVertex),
                          reinterpret_cast<const void*>(offsetof(aegis::ModelVertex, normal)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);
    drawMesh.indexCount = static_cast<GLsizei>(mesh.indices.size());
}

bool InitDrone(Drone& drone, const std::filesystem::path& objDirectory)
{
    const auto materialPath = objDirectory / "materials.mtl";
    if (!std::ifstream(materialPath)) {
        std::cerr << "Could not read drone materials: " << materialPath << '\n';
        return false;
    }

    for (auto& part : drone) {
        for (const char* materialName : part.materials) {
            const auto path = objDirectory / (std::string("drone_") + part.name +
                                              '_' + materialName + ".obj");
            aegis::ModelData model;
            std::string error;
            if (!aegis::LoadModel(path, model, error)) {
                std::cerr << error << '\n';
                return false;
            }

            for (const auto& mesh : model.meshes) {
                const auto& material = model.materials[mesh.materialIndex];
                if (material.name != materialName) {
                    std::cerr << path << ": expected material " << materialName << " from "
                              << materialPath << ", found " << material.name << '\n';
                    return false;
                }
                auto& drawMesh = part.meshes.emplace_back();
                drawMesh.color = material.diffuse;
                InitVertexBuffer(drawMesh, mesh);
            }
        }
    }
    return true;
}

void DrawScene(const Drone& drone, GLuint shaderProgramID, GLint transformLocation,
               GLint colorLocation, int width, int height)
{
    glViewport(0, 0, width, height);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glUseProgram(shaderProgramID);

    // 투영 행렬과 카메라 행렬은 모든 부품에 공통으로 사용한다.
    const float aspect = static_cast<float>(width) / static_cast<float>(height);
    const glm::mat4 projection = glm::perspective(glm::radians(33.0f), aspect, 0.1f, 20.0f);
    const glm::vec3 camPos(1.5f, 1.85f, 3.4f);
    const glm::vec3 camTarget(0.13f, -0.025f, 0.02f);
    const glm::vec3 camUp(0.0f, 1.0f, 0.0f);
    const glm::mat4 scene = projection * glm::lookAt(camPos, camTarget, camUp);

    for (const auto& part : drone) {
        // 부품의 로컬 좌표를 조립 원점만큼 옮긴다.
        const glm::mat4 transform = glm::translate(scene, part.origin);
        glUniformMatrix4fv(transformLocation, 1, GL_FALSE, glm::value_ptr(transform));
        for (const auto& mesh : part.meshes) {
            glUniform3fv(colorLocation, 1, glm::value_ptr(mesh.color));
            glBindVertexArray(mesh.vao);
            glDrawElements(GL_TRIANGLES, mesh.indexCount, GL_UNSIGNED_INT, nullptr);
        }
    }
}

void ClearDrone(const Drone& drone)
{
    for (const auto& part : drone) {
        for (const auto& mesh : part.meshes) {
            glDeleteVertexArrays(1, &mesh.vao);
            glDeleteBuffers(1, &mesh.vbo);
            glDeleteBuffers(1, &mesh.ebo);
        }
    }
}

int RunWindow()
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
    GLFWwindow* window = glfwCreateWindow(960, 540, "AEGIS - OpenGL", nullptr, nullptr);
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

    // assembly.json의 그룹 원점이다. 각 OBJ는 그룹 기준 로컬 좌표를 미터로 저장한다.
    Drone drone{{
        {"body",        { 0.00f,  0.000f,  0.00f}, {"shell", "white", "dark", "steel", "red"}},
        {"sensor",      { 0.00f, -0.015f,  0.49f}, {"rubber", "steel", "red", "lens"}},
        {"rotor_left",  {-1.02f,  0.040f, -0.02f}, {"dark", "rubber", "steel"}},
        {"rotor_right", { 1.02f,  0.040f, -0.02f}, {"dark", "rubber", "steel"}},
        {"weapon",      { 0.00f, -0.350f,  0.13f}, {"shell", "dark", "rubber", "steel", "red"}},
    }};
    const GLuint shaderProgramID = MakeShaderProgram(projectDirectory / "game" / "shaders");
    const bool ready = shaderProgramID &&
                       InitDrone(drone, projectDirectory / "asset" / "drone" / "obj");
    if (ready) {
        const GLint transformLocation = glGetUniformLocation(shaderProgramID, "uTransform");
        const GLint colorLocation = glGetUniformLocation(shaderProgramID, "uColor");
        std::cout << "OpenGL " << glGetString(GL_VERSION) << std::endl;
        glfwSwapInterval(1);
        glEnable(GL_DEPTH_TEST);
        glClearColor(0.16f, 0.19f, 0.24f, 1.0f);
        while (!glfwWindowShouldClose(window)) {
            glfwPollEvents();
            if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
                glfwSetWindowShouldClose(window, GLFW_TRUE);
            }

            int width{}, height{};
            glfwGetFramebufferSize(window, &width, &height);
            if (width > 0 && height > 0) {
                DrawScene(drone, shaderProgramID, transformLocation, colorLocation, width, height);
                glfwSwapBuffers(window);
            } else {
                glfwWaitEventsTimeout(0.1);
            }
        }
    }

    // 창을 닫기 전에 컨텍스트에서 버퍼와 셰이더 프로그램을 해제한다.
    ClearDrone(drone);
    if (shaderProgramID) {
        glUseProgram(0);
        glDeleteProgram(shaderProgramID);
    }
    glfwDestroyWindow(window);
    glfwTerminate();
    return ready ? 0 : 1;
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
