#include "combat_test_scene.h"

#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>
#include <fstream>
#include <iostream>
#include <vector>
#include <array>

namespace aegis {
namespace {
GLuint Compile(GLenum kind, const char* source)
{
    const GLuint shader = glCreateShader(kind);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    GLint ok{};
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[2048]{};
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        std::cerr << "Combat range shader: " << log << '\n';
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}
}

bool CombatTestScene::Init(const std::filesystem::path& projectDirectory)
{
    const char* vertex = R"(#version 330 core
layout(location=0) in vec3 position;
uniform mat4 transform;
out vec3 localPosition;
void main() { localPosition=position; gl_Position=transform*vec4(position,1); }
)";
    const char* fragment = R"(#version 330 core
in vec3 localPosition;
uniform vec3 color;
out vec4 outColor;
void main() {
    vec3 normal=normalize(cross(dFdx(localPosition),dFdy(localPosition)));
    float light=0.65+0.35*abs(dot(normal,normalize(vec3(0.4,0.8,0.5))));
    outColor=vec4(color*light,1);
})";
    const GLuint vs = Compile(GL_VERTEX_SHADER, vertex);
    const GLuint fs = Compile(GL_FRAGMENT_SHADER, fragment);
    if (!vs || !fs) { if (vs) glDeleteShader(vs); if (fs) glDeleteShader(fs); return false; }
    program = glCreateProgram();
    glAttachShader(program, vs); glAttachShader(program, fs); glLinkProgram(program);
    glDeleteShader(vs); glDeleteShader(fs);
    GLint linked{}; glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (!linked) { Clear(); return false; }
    transformLocation = glGetUniformLocation(program, "transform");
    colorLocation = glGetUniformLocation(program, "color");
    const glm::vec3 corners[] = {
        {-0.5f,-0.5f,-0.5f},{0.5f,-0.5f,-0.5f},{0.5f,0.5f,-0.5f},{-0.5f,0.5f,-0.5f},
        {-0.5f,-0.5f,0.5f},{0.5f,-0.5f,0.5f},{0.5f,0.5f,0.5f},{-0.5f,0.5f,0.5f}};
    const unsigned indices[] = {0,2,1,0,3,2,4,5,6,4,6,7,0,4,7,0,7,3,
        1,2,6,1,6,5,0,1,5,0,5,4,3,7,6,3,6,2};
    std::vector<glm::vec3> vertices;
    for (unsigned index : indices) vertices.push_back(corners[index]);
    glGenVertexArrays(1, &vao); glGenBuffers(1, &vbo);
    glBindVertexArray(vao); glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, vertices.size()*sizeof(glm::vec3), vertices.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,sizeof(glm::vec3),nullptr);
    glEnableVertexAttribArray(0); glBindVertexArray(0);
    Reset();
    if (!weapon.Init(projectDirectory)) { Clear(); return false; }
    return true;
}

void CombatTestScene::Reset()
{
    range.Reset(); encounter.Reset(); weapon.Reset(); yaw = pitch = 0;
    camera.SetPose({0,0,0}, yaw, pitch);
    // Held input remains latched until release (R must not create a fresh shot).
    lookHeld = false;
}

void CombatTestScene::SetTurretMode(bool enabled)
{
    turretMode=enabled;
    Reset(); smokeFailureSeen=false;
}

bool CombatTestScene::Fire(const glm::vec3& direction)
{
    const bool fired=turretMode ? encounter.Fire(camera.EyePosition(),direction)
        : range.Fire(camera.EyePosition(),direction);
    if (fired) weapon.OnShot();
    return fired;
}

void CombatTestScene::SampleInput(GLFWwindow* window, bool active)
{
    const bool left = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
    const bool right = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;
    const bool cover = glfwGetKey(window, GLFW_KEY_B) == GLFW_PRESS;
    const bool turret = glfwGetKey(window, GLFW_KEY_T) == GLFW_PRESS;
    double x{}, y{}; glfwGetCursorPos(window, &x, &y);
    if (active) {
        if (turret && !turretHeld) SetTurretMode(!turretMode);
        const bool playing=!turretMode || encounter.Result()==combat::EncounterResult::Running;
        if (playing && right && lookHeld) {
            yaw -= static_cast<float>(x-lastX)*0.14f;
            pitch = std::clamp(pitch-static_cast<float>(y-lastY)*0.14f, -65.0f,65.0f);
            camera.SetPose({0,0,0}, yaw,pitch);
        }
        if (playing && cover && !coverHeld) {
            if (turretMode) encounter.ToggleCover(); else range.ToggleCover();
        }
        if (playing && left && !mouseHeld) {
            Fire(camera.ViewDirection());
        }
    }
    mouseHeld = left; coverHeld = cover; lookHeld = active && right;
    turretHeld=turret;
    lastX=x; lastY=y;
}

void CombatTestScene::Update(double realDt, double gameDt)
{
    if (turretMode) encounter.Update(static_cast<float>(realDt),static_cast<float>(gameDt),camera.EyePosition());
    else range.Update(static_cast<float>(realDt), static_cast<float>(gameDt));
    weapon.Update(realDt);
}

void CombatTestScene::DrawBox(const glm::mat4& vp, const glm::vec3& minimum,
    const glm::vec3& maximum, const glm::vec3& color) const
{
    const auto model = glm::scale(glm::translate(glm::mat4(1),(minimum+maximum)*0.5f),maximum-minimum);
    const auto transform = vp*model;
    glUniformMatrix4fv(transformLocation,1,GL_FALSE,glm::value_ptr(transform));
    glUniform3fv(colorLocation,1,glm::value_ptr(color));
    glDrawArrays(GL_TRIANGLES,0,36);
}

void CombatTestScene::Draw(int width, int height) const
{
    if (width <= 0 || height <= 0) return;
    glViewport(0,0,width,height); glDisable(GL_FRAMEBUFFER_SRGB); glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE); glEnable(GL_DEPTH_TEST); glDepthMask(GL_TRUE);
    glClearColor(0.025f,0.04f,0.065f,1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    const auto vp=glm::perspective(glm::radians(60.0f),static_cast<float>(width)/height,0.05f,80.0f)*camera.ViewMatrix();
    glUseProgram(program); glBindVertexArray(vao);
    const std::span<const combat::Collider> boxes=turretMode
        ? std::span<const combat::Collider>(encounter.Colliders())
        : std::span<const combat::Collider>(range.Colliders());
    for (const auto& box : boxes) {
        if (!box.enabled) continue;
        const glm::vec3 color = box.id==500 ? glm::vec3(0.28f,0.34f,0.43f)
            : box.kind == combat::HitKind::Body ? glm::vec3(0.05f,0.9f,0.7f)
            : box.id == 200 ? glm::vec3(0.95f,0.35f,0.12f) : glm::vec3(0.15f,0.22f,0.31f);
        DrawBox(vp,box.minimum,box.maximum,color);
    }
    if (turretMode && encounter.Result()!=combat::EncounterResult::Cleared) {
        DrawBox(vp,{-0.9f,0,-10.6f},{0.9f,0.35f,-9.3f},{0.25f,0.3f,0.4f});
        DrawBox(vp,{-0.16f,1.49f,-9.5f},{0.16f,1.81f,-9.25f},{0.12f,0.16f,0.22f});
        const auto sensor=encounter.Phase()==combat::TurretPhase::Warning
            ? glm::vec3(1,0.15f,0.1f) : glm::vec3(0.2f,0.7f,1);
        DrawBox(vp,{-0.28f,1.94f,-9.52f},{0.28f,2.12f,-9.46f},sensor);
    }
    // Floor strips make depth and aiming changes visible without extra assets.
    for (int z=0;z>=-12;--z) DrawBox(vp,{-5,0.005f,static_cast<float>(z)-0.015f},
        {5,0.012f,static_cast<float>(z)+0.015f},{0.23f,0.35f,0.44f});
    const auto& shots=turretMode ? encounter.PlayerFlights() : range.Flights();
    for (const auto& flight : shots) {
        const auto p=flight.projectile.position;
        DrawBox(vp,p-glm::vec3(0.055f),p+glm::vec3(0.055f),{1,0.85f,0.15f});
    }
    if (turretMode) for (const auto& flight:encounter.EnemyFlights()) {
        const auto p=flight.projectile.position;
        DrawBox(vp,p-glm::vec3(0.08f),p+glm::vec3(0.08f),{1,0.15f,0.08f});
    }
    if (!turretMode && range.LastHit() && range.ImpactTime()>0) {
        const auto p=range.LastHit()->position;
        DrawBox(vp,p-glm::vec3(0.07f),p+glm::vec3(0.07f),{1,1,1});
    }
    glBindVertexArray(0); glUseProgram(0);
    weapon.Draw(static_cast<float>(width)/height);
    // Crisp centre crosshair drawn in framebuffer pixels.
    glEnable(GL_SCISSOR_TEST);
    glClearColor(range.RemainingTargets()==0 ? 0.15f : 0.95f,1,0.7f,1);
    glScissor(width/2-8,height/2-1,17,3); glClear(GL_COLOR_BUFFER_BIT);
    glScissor(width/2-1,height/2-8,3,17); glClear(GL_COLOR_BUFFER_BIT);
    DrawHud(width,height);
    glDisable(GL_SCISSOR_TEST);
}

void CombatTestScene::Clear()
{
    weapon.Clear();
    if (vbo) glDeleteBuffers(1,&vbo);
    if (vao) glDeleteVertexArrays(1,&vao);
    if (program) glDeleteProgram(program);
    vbo=vao=program=0;
}

void CombatTestScene::SetSmokeFrame(int frame)
{
    if (turretMode) {
        if (frame==660) {
            smokeFailureSeen=encounter.Result()==combat::EncounterResult::Failed;
            Reset();
        }
        if (frame==661) Fire(camera.ViewDirection());
        return;
    }
    if (frame==1) Fire({0,0,-1});
    if (frame==70) { range.ToggleCover(); Fire({0,0,-1}); }
    if (frame==140) Fire(glm::vec3(-2.1f,1.6f,-9.8f)-camera.EyePosition());
    if (frame==210) Fire(glm::vec3(2.1f,1.6f,-9.8f)-camera.EyePosition());
}

bool CombatTestScene::SmokePassed() const
{
    if (turretMode) return smokeFailureSeen && encounter.Result()==combat::EncounterResult::Cleared
        && encounter.Health()==100 && encounter.PlayerFlights().empty() && encounter.EnemyFlights().empty()
        && weapon.ShotCount()==encounter.PlayerShots();
    return range.RemainingTargets()==0 && range.Shots()==4 && range.BlockedShots()==1 && range.Flights().empty()
        && weapon.ShotCount()==range.Shots();
}

std::string CombatTestScene::StatusTitle(bool paused) const
{
    if (!turretMode) return std::string("AEGIS RANGE | ")+(paused ? "PAUSED | " : "")
        +"Targets: "+std::to_string(range.RemainingTargets())+"/3 | Shots: "+std::to_string(range.Shots())
        +" | Blocked: "+std::to_string(range.BlockedShots())+" | Cover: "+(range.CoverEnabled()?"ON":"OFF")
        +" | T turret / LMB fire / RMB aim / B cover / R reset / F2 drone";
    const char* state=encounter.Result()==combat::EncounterResult::Failed ? "FAILED"
        : encounter.Result()==combat::EncounterResult::Cleared ? "CLEARED"
        : encounter.Phase()==combat::TurretPhase::Warning ? "WARNING"
        : encounter.Phase()==combat::TurretPhase::Cooldown ? "COOLDOWN" : "READY";
    return std::string("AEGIS TURRET | ")+(paused ? "PAUSED | " : "")+state
        +" | HP: "+std::to_string(encounter.Health())+" | Hits: "+std::to_string(encounter.HitsTaken())
        +" | EnemyShots: "+std::to_string(encounter.EnemyShots())+" | PlayerShots: "+std::to_string(encounter.PlayerShots())
        +" | Cover: "+(encounter.CoverEnabled()?"ON":"OFF")+" | B cover / R retry / T range / F2 drone";
}

bool CombatTestScene::SaveFrame(const std::filesystem::path& path, int width, int height) const
{
    std::vector<unsigned char> pixels(static_cast<std::size_t>(width)*height*3);
    glPixelStorei(GL_PACK_ALIGNMENT,1); glReadBuffer(GL_BACK);
    glReadPixels(0,0,width,height,GL_RGB,GL_UNSIGNED_BYTE,pixels.data());
    glPixelStorei(GL_PACK_ALIGNMENT,4);
    std::ofstream output(path,std::ios::binary);
    output << "P6\n" << width << ' ' << height << "\n255\n";
    for (int row=height-1;row>=0;--row)
        output.write(reinterpret_cast<const char*>(pixels.data()+static_cast<std::size_t>(row)*width*3),width*3);
    return static_cast<bool>(output);
}

} // namespace aegis
