#include "combat_test_scene.h"
#include "corridor_layout.generated.h"
#include "terminal_layout.generated.h"

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
    if (!weapon.Init(projectDirectory) || !droneRenderer.Init(projectDirectory/"game/shaders") || !drone.Init(projectDirectory) || !InitTerminal(projectDirectory) || !bossView.Init(projectDirectory)) { Clear(); return false; }
    return true;
}

bool CombatTestScene::InitTerminal(const std::filesystem::path& root) {
    ModelData model; std::string error;
    if (!LoadModel(root/terminal_asset::mesh,model,error)) { std::cerr << error << '\n'; return false; }
    std::size_t triangles=0;
    for (const auto& mesh:model.meshes) {
        triangles+=mesh.indices.size()/3;
        auto& gpu=terminalMeshes.emplace_back(); gpu.Upload(mesh,model.materials.at(mesh.materialIndex));
    }
    return triangles==terminal_asset::triangles;
}

void CombatTestScene::Reset()
{
    analysis.Reset();
    range.Reset(); encounter.Reset(); weapon.Reset(); drone.Reset(); boss.Reset(); bossCovered=false; yaw = pitch = 0;
    if (bossMode) { const auto d=glm::normalize(boss.CoreCenter()-(CameraRoot()+glm::vec3(0,1.65f,0))); pitch=glm::degrees(std::asin(d.y)); }
    camera.SetPose(CameraRoot(), yaw, pitch);
    // Held input remains latched until release (R must not create a fresh shot).
    lookHeld = false;
}

void CombatTestScene::SetCorridorMode(bool enabled) {
    bossMode=false;
    corridorMode=enabled;
    std::vector<combat::Collider> walls;
    if (enabled) { walls.assign(corridor_asset::walls.begin(),corridor_asset::walls.end()); walls.push_back(terminal_asset::collider); }
    encounter.SetEnvironment(walls);
    encounter.SetExitDoors(enabled ? 10010:0,enabled ? 10011:0,enabled ? corridor_asset::doorTravel:0);
    encounter.SetDroneMode(enabled);
    SetTurretMode(enabled);
}

void CombatTestScene::SetDroneMode(bool enabled) {
    encounter.SetDroneMode(enabled); Reset();
    smokeFailureSeen=smokePatrolSeen=smokeWarningSeen=smokeShotSeen=smokeSecuringSeen=smokeAnalysisSeen=smokeAnalysisReturned=smokeWorldFrozen=false;
}

void CombatTestScene::SetTurretMode(bool enabled)
{
    turretMode=enabled;
    Reset(); smokeFailureSeen=false;
}

glm::vec3 CombatTestScene::CameraRoot() const { return bossMode ? (bossCovered ? combat::BossEncounter::CoverRoot():combat::BossEncounter::FiringRoot()):glm::vec3(0); }
void CombatTestScene::SetBossMode(bool enabled) {
    bossMode=enabled; Reset(); smokeBossFailureSeen=false;
}
void CombatTestScene::ToggleBossCover() {
    bossCovered=!bossCovered;
    const auto target=bossCovered ? boss_asset::supplyInteraction:boss.CoreCenter();
    const auto d=glm::normalize(target-(CameraRoot()+glm::vec3(0,1.65f,0)));
    yaw=glm::degrees(std::atan2(-d.x,-d.z)); pitch=glm::degrees(std::asin(d.y));
    camera.SetPose(CameraRoot(),yaw,pitch); lookHeld=false;
}

bool CombatTestScene::Fire(const glm::vec3& direction)
{
    const bool fired=bossMode ? boss.Fire(camera.EyePosition(),direction):turretMode ? encounter.Fire(camera.EyePosition(),direction)
        : range.Fire(camera.EyePosition(),direction);
    if (fired) weapon.OnShot();
    return fired;
}

const combat::WeaponInventory& CombatTestScene::Inventory() const { return bossMode ? boss.Inventory():turretMode ? encounter.Inventory() : range.Inventory(); }
bool CombatTestScene::Interact(const glm::vec3& direction) {
    if (bossMode) return boss.Interact(camera.EyePosition(),direction);
    if (TerminalTarget(direction)) return analysis.Open(CanAnalyze(),terminal_asset::collider,camera.EyePosition(),direction,encounter.Colliders());
    return turretMode ? encounter.Interact(camera.EyePosition(),direction) : range.Interact(camera.EyePosition(),direction);
}
bool CombatTestScene::TerminalTarget() const { return TerminalTarget(camera.ViewDirection()); }
bool CombatTestScene::TerminalTarget(const glm::vec3& direction) const {
    if (!corridorMode || bossMode) return false;
    return analysis.IsFirstTarget(terminal_asset::collider,camera.EyePosition(),direction,encounter.Colliders(),
        Inventory().InteractionDistance(camera.EyePosition(),direction,encounter.Colliders()));
}
combat::WeaponInteraction CombatTestScene::InteractionHint() const {
    if (bossMode) return boss.InteractionHint(camera.EyePosition(),camera.ViewDirection());
    if (TerminalTarget()) return combat::WeaponInteraction::None;
    return turretMode ? encounter.InteractionHint(camera.EyePosition(),camera.ViewDirection())
        : range.InteractionHint(camera.EyePosition(),camera.ViewDirection());
}

void CombatTestScene::SampleInput(GLFWwindow* window, bool active)
{
    const bool left = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
    const bool right = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;
    const bool cover = glfwGetKey(window, GLFW_KEY_B) == GLFW_PRESS;
    const bool drop = glfwGetKey(window, GLFW_KEY_G) == GLFW_PRESS;
    const bool interact = glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS;
    const bool bossToggle = glfwGetKey(window, GLFW_KEY_F4) == GLFW_PRESS;
    const bool droneToggle = glfwGetKey(window, GLFW_KEY_F3) == GLFW_PRESS;
    const bool turret = glfwGetKey(window, GLFW_KEY_T) == GLFW_PRESS;
    double x{}, y{}; glfwGetCursorPos(window, &x, &y);
    if (analysis.Active()) {
        if (active && interact && !interactHeld) analysis.Close();
        mouseHeld=left; coverHeld=cover; lookHeld=false; turretHeld=turret;
        droneToggleHeld=droneToggle; bossToggleHeld=bossToggle; dropHeld=drop; interactHeld=interact; lastX=x; lastY=y;
        return;
    }
    if (active) {
        if (bossToggle && !bossToggleHeld) SetBossMode(!bossMode);
        if (!bossMode && corridorMode && droneToggle && !droneToggleHeld) SetDroneMode(!IsDroneMode());
        if (!bossMode && !corridorMode && turret && !turretHeld) SetTurretMode(!turretMode);
        const bool playing=bossMode ? boss.Result()==combat::EncounterResult::Running || boss.Result()==combat::EncounterResult::Securing : !turretMode || encounter.Result()==combat::EncounterResult::Running || encounter.Result()==combat::EncounterResult::Securing;
        const bool canEquip=bossMode ? boss.Result()!=combat::EncounterResult::Failed : !turretMode || encounter.Result()!=combat::EncounterResult::Failed;
        if (canEquip && right && lookHeld) {
            yaw -= static_cast<float>(x-lastX)*0.14f;
            pitch = std::clamp(pitch-static_cast<float>(y-lastY)*0.14f, -65.0f,65.0f);
            camera.SetPose(CameraRoot(), yaw,pitch);
        }
        if (playing && cover && !coverHeld) {
            if (bossMode) ToggleBossCover();
            else if (turretMode) encounter.ToggleCover(); else range.ToggleCover();
        }
        if (canEquip && drop && !dropHeld) {
            if (bossMode) boss.DropWeapon(camera.EyePosition(),camera.ViewDirection());
            else if (turretMode) encounter.DropWeapon(camera.EyePosition(),camera.ViewDirection());
            else range.DropWeapon(camera.EyePosition(),camera.ViewDirection());
        }
        if (canEquip && interact && !interactHeld) Interact(camera.ViewDirection());
        if (playing && left && !mouseHeld) {
            Fire(camera.ViewDirection());
        }
    }
    mouseHeld = left; coverHeld = cover; lookHeld = active && right;
    turretHeld=turret; droneToggleHeld=droneToggle; bossToggleHeld=bossToggle; dropHeld=drop; interactHeld=interact;
    lastX=x; lastY=y;
}

void CombatTestScene::Update(double realDt, double gameDt)
{
    if (analysis.Active()) return;
    if (bossMode) boss.Update(static_cast<float>(realDt),static_cast<float>(gameDt),camera.EyePosition(),camera.ViewDirection());
    else if (turretMode) encounter.Update(static_cast<float>(realDt),static_cast<float>(gameDt),camera.EyePosition(),camera.ViewDirection());
    else range.Update(static_cast<float>(realDt), static_cast<float>(gameDt));
    if (IsDroneMode() && encounter.Result()==combat::EncounterResult::Running) drone.UpdateCombat(gameDt);
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

void CombatTestScene::Draw(int width, int height,
    const std::function<void(const glm::mat4&,const glm::vec3&)>& drawEnvironment) const
{
    if (width <= 0 || height <= 0) return;
    if (analysis.Active()) { DrawAnalysis(width,height); return; }
    glViewport(0,0,width,height); glDisable(GL_FRAMEBUFFER_SRGB); glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE); glEnable(GL_DEPTH_TEST); glDepthMask(GL_TRUE);
    glClearColor(0.025f,0.04f,0.065f,1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    const auto projection=glm::perspective(glm::radians(60.0f),static_cast<float>(width)/height,0.05f,80.0f);
    const auto vp=projection*camera.ViewMatrix();
    if (bossMode) bossView.Draw(projection,camera.ViewMatrix(),camera.EyePosition(),boss);
    else if (drawEnvironment) drawEnvironment(vp,camera.EyePosition());
    if (corridorMode && !bossMode) {
        droneRenderer.Begin(camera.EyePosition(),{0.45f,0.9f,0.7f},{1,1,1},3,0.3f);
        droneRenderer.Draw(terminalMeshes,vp,glm::translate(glm::mat4(1),terminal_asset::position));
    }
    if (IsDroneMode() && encounter.EnemyAlive()) {
        const auto& state=encounter.DroneState();
        const auto world=glm::rotate(glm::translate(glm::mat4(1),state.Position()),state.Yaw(),glm::vec3(0,1,0));
        droneRenderer.Begin(camera.EyePosition(),{0.45f,0.9f,0.7f},{1,1,1},4,0.25f);
        drone.Draw(droneRenderer,vp,world,state.Phase()==combat::DronePhase::Warning,state.Armed());
    }
    glUseProgram(program); glBindVertexArray(vao);
    const std::span<const combat::Collider> boxes=bossMode ? std::span<const combat::Collider>() : turretMode
        ? std::span<const combat::Collider>(encounter.Colliders())
        : std::span<const combat::Collider>(range.Colliders());
    for (const auto& box : boxes) {
        if ((corridorMode && box.id==12000) || (IsDroneMode() && (box.id==500 || box.id==501)) || !box.enabled || (corridorMode && box.kind==combat::HitKind::Wall)) continue;
        const glm::vec3 color = box.id==500 ? glm::vec3(0.28f,0.34f,0.43f)
            : box.kind == combat::HitKind::Weapon ? glm::vec3(0.9f,0.7f,0.2f)
            : box.kind == combat::HitKind::Body ? glm::vec3(0.05f,0.9f,0.7f)
            : box.id == 200 ? glm::vec3(0.95f,0.35f,0.12f) : glm::vec3(0.15f,0.22f,0.31f);
        DrawBox(vp,box.minimum,box.maximum,color);
    }
    if (turretMode && !bossMode && !IsDroneMode() && encounter.EnemyAlive()) {
        DrawBox(vp,{-0.9f,0,-10.6f},{0.9f,0.35f,-9.3f},{0.25f,0.3f,0.4f});

        const auto sensor=encounter.Phase()==combat::TurretPhase::Warning
            ? glm::vec3(1,0.15f,0.1f) : glm::vec3(0.2f,0.7f,1);
        DrawBox(vp,{-0.28f,1.94f,-9.52f},{0.28f,2.12f,-9.46f},sensor);
    }
    // Floor strips make depth and aiming changes visible without extra assets.
    if (!bossMode && !corridorMode) for (int z=0;z>=-12;--z) DrawBox(vp,{-5,0.005f,static_cast<float>(z)-0.015f},
        {5,0.012f,static_cast<float>(z)+0.015f},{0.23f,0.35f,0.44f});
    const auto& shots=bossMode ? boss.PlayerFlights() : turretMode ? encounter.PlayerFlights() : range.Flights();
    for (const auto& flight : shots) {
        const auto p=flight.projectile.position;
        DrawBox(vp,p-glm::vec3(0.055f),p+glm::vec3(0.055f),{1,0.85f,0.15f});
    }
    if (bossMode || turretMode) for (const auto& flight:(bossMode ? boss.EnemyFlights():encounter.EnemyFlights())) {
        const auto p=flight.projectile.position;
        DrawBox(vp,p-glm::vec3(0.08f),p+glm::vec3(0.08f),{1,0.15f,0.08f});
    }
    if (!bossMode && !turretMode && range.LastHit() && range.ImpactTime()>0) {
        const auto p=range.LastHit()->position;
        DrawBox(vp,p-glm::vec3(0.07f),p+glm::vec3(0.07f),{1,1,1});
    }
    const auto supply=bossMode ? boss_asset::supplyWeapon:combat::WeaponInventory::SupplyPosition();
    if (!bossMode) DrawBox(vp,supply+glm::vec3(-0.32f,-0.5f,-0.27f),supply+glm::vec3(0.32f,-0.15f,0.27f),{0.1f,0.55f,0.38f});
    glBindVertexArray(0); glUseProgram(0);
    weapon.DrawWorld(projection,camera.ViewMatrix(),supply,bossMode ? 0.0f:90.0f);
    for (const auto& drop:Inventory().Drops()) weapon.DrawWorld(projection,camera.ViewMatrix(),drop.position);
    if (Inventory().Armed()) weapon.Draw(static_cast<float>(width)/height);
    // Crisp centre crosshair drawn in framebuffer pixels.
    glEnable(GL_SCISSOR_TEST);
    glClearColor(range.RemainingTargets()==0 ? 0.15f : 0.95f,1,0.7f,1);
    glScissor(width/2-8,height/2-1,17,3); glClear(GL_COLOR_BUFFER_BIT);
    glScissor(width/2-1,height/2-8,3,17); glClear(GL_COLOR_BUFFER_BIT);
    DrawHud(width,height);
    glDisable(GL_SCISSOR_TEST);
}

void CombatTestScene::DrawAnalysis(int width,int height) const {
    glViewport(0,0,width,height); glDisable(GL_FRAMEBUFFER_SRGB); glDisable(GL_SCISSOR_TEST); glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST); glDepthMask(GL_TRUE); glDisable(GL_CULL_FACE);
    glClearColor(0.025f,0.055f,0.09f,1); glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    const glm::vec3 eye{3.2f,1.8f,4.5f};
    const auto vp=glm::perspective(glm::radians(45.0f),static_cast<float>(width)/height,0.05f,30.0f)
        *glm::lookAt(eye,glm::vec3(0),glm::vec3(0,1,0));
    droneRenderer.Begin(eye,{0.45f,0.9f,0.7f},{1,1,1},5,0.3f);
    drone.Draw(droneRenderer,vp,glm::mat4(1),false,true,true);
    glEnable(GL_SCISSOR_TEST); DrawHud(width,height); glDisable(GL_SCISSOR_TEST);
}

void CombatTestScene::Clear()
{
    analysis.Reset(); bossView.Clear();
    for (auto& mesh:terminalMeshes) mesh.Clear();
    terminalMeshes.clear();
    drone.Clear(); droneRenderer.Clear();
    weapon.Clear();
    if (vbo) glDeleteBuffers(1,&vbo);
    if (vao) glDeleteVertexArrays(1,&vao);
    if (program) glDeleteProgram(program);
    vbo=vao=program=0;
}

void CombatTestScene::SetSmokeFrame(int frame)
{
    if (bossMode) {
        if (frame==900) { smokeBossFailureSeen=boss.Result()==combat::EncounterResult::Failed; Reset(); }
        if (frame==901) { Fire(boss.CoreCenter()-camera.EyePosition()); ToggleBossCover(); }
        if (frame>901 && boss.Result()==combat::EncounterResult::Running) {
            if (boss.Phase()==combat::BossPhase::Exposed && Inventory().Armed()) {
                if (bossCovered) ToggleBossCover();
                if (boss.PlayerFlights().empty()) Fire(boss.CoreCenter()-camera.EyePosition());
            } else {
                if (!bossCovered) ToggleBossCover();
                if (!Inventory().Armed()) Interact(boss_asset::supplyInteraction-camera.EyePosition());
            }
        }
        return;
    }
    if (IsDroneMode()) {
        smokePatrolSeen=smokePatrolSeen || encounter.DroneState().Position().x>-0.5f;
        smokeWarningSeen=smokeWarningSeen || encounter.DroneState().Phase()==combat::DronePhase::Warning;
        smokeShotSeen=smokeShotSeen || encounter.EnemyShots()>0;
        if (frame==900) { smokeFailureSeen=encounter.Result()==combat::EncounterResult::Failed; Reset(); }
        smokeSecuringSeen=smokeSecuringSeen || encounter.Result()==combat::EncounterResult::Securing;
        if (frame==1150) { smokeDoorSnapshot=DoorProgress(); Interact(terminal_asset::interactionPoint-camera.EyePosition()); }
        if (frame==1160) { smokeAnalysisSeen=analysis.Active(); smokeWorldFrozen=DoorProgress()==smokeDoorSnapshot && smokeDoorSnapshot<1; }
        if (frame==1190) analysis.Close();
        if (frame==1191) smokeAnalysisReturned=!analysis.Active() && analysis.Entries()>0 && analysis.Exits()>0;
        if (frame==1060) Fire(encounter.DroneState().Position()-camera.EyePosition());
        return;
    }
    if (turretMode) {
        if (frame==660) {
            smokeFailureSeen=encounter.Result()==combat::EncounterResult::Failed;
            Reset();
        }
        if (frame==661) Fire(camera.ViewDirection());
        return;
    }
    if (frame==1) Fire({0,0,-1});
    if (frame==45) range.DropWeapon(camera.EyePosition(),camera.ViewDirection());
    if (frame==50 && !range.Inventory().Drops().empty()) Interact(range.Inventory().Drops()[0].position-camera.EyePosition());
    if (frame==70) { range.ToggleCover(); Fire({0,0,-1}); }
    if (frame==130 || frame==200) Interact(combat::WeaponInventory::SupplyPosition()-camera.EyePosition());
    if (frame==140) Fire(glm::vec3(-2.1f,1.6f,-9.8f)-camera.EyePosition());
    if (frame==210) Fire(glm::vec3(2.1f,1.6f,-9.8f)-camera.EyePosition());
}

bool CombatTestScene::SmokePassed() const
{
    if (bossMode) return smokeBossFailureSeen && boss.Result()==combat::EncounterResult::Cleared
        && boss.CoreHits()==3 && boss.Health()==100 && boss.PlayerShots()==4 && boss.BlockedShots()>=1
        && Inventory().Breaks()==3 && Inventory().Supplies()==2 && !Inventory().Armed()
        && weapon.ShotCount()==boss.PlayerShots() && boss.EnemyFlights().empty();
    if (IsDroneMode() && (!smokePatrolSeen || !smokeWarningSeen || !smokeShotSeen || !smokeSecuringSeen || !smokeAnalysisSeen || !smokeAnalysisReturned || !smokeWorldFrozen)) return false;
    if (turretMode) return smokeFailureSeen && encounter.Result()==combat::EncounterResult::Cleared
        && encounter.Health()==(IsDroneMode()?75:100) && encounter.PlayerFlights().empty() && encounter.EnemyFlights().empty()
        && weapon.ShotCount()==encounter.PlayerShots() && !Inventory().Armed() && Inventory().Breaks()==1
        && Inventory().Drops().size()==1 && encounter.CanAnalyze()
        && (!corridorMode || encounter.DoorProgress()>=1);
    return range.RemainingTargets()==0 && range.Shots()==4 && range.BlockedShots()==1 && range.Flights().empty()
        && weapon.ShotCount()==range.Shots() && !Inventory().Armed()
        && Inventory().Breaks()==3 && Inventory().Supplies()==2 && Inventory().Pickups()==1 && Inventory().Drops().size()==3;
}

std::string CombatTestScene::StatusTitle(bool paused) const
{
    if (bossMode) return std::string("AEGIS BOSS | ")+(paused ? "PAUSED | ":"")
        +(boss.Result()==combat::EncounterResult::Failed ? "FAILED":boss.Result()==combat::EncounterResult::Cleared ? "VICTORY":boss.Result()==combat::EncounterResult::Securing ? "SECURING":boss.Phase()==combat::BossPhase::Exposed ? "CORE OPEN":boss.Phase()==combat::BossPhase::Warning ? "WARNING":"ARMOR CLOSED")
        +" | Core: "+std::to_string(boss.CoreHits())+"/3 | HP: "+std::to_string(boss.Health())
        +" | Shots: "+std::to_string(boss.PlayerShots())+" | Blocked: "+std::to_string(boss.BlockedShots())
        +" | Weapon: "+(Inventory().Armed()?"READY":"EMPTY")+" | Supplies: "+std::to_string(Inventory().Supplies())
        +" | Cover: "+(bossCovered?"ON":"OFF")+" | B test position / E refill / R reset / F4 corridor";
    if (analysis.Active()) return "AEGIS ANALYSIS | World: PAUSED | Enter: "+std::to_string(analysis.Entries())
        +" | Exit: "+std::to_string(analysis.Exits())+" | E or Esc return";
    const std::string weaponStatus=std::string(" | Weapon: ")+(Inventory().Armed()?"READY":"EMPTY")
        +" | Breaks: "+std::to_string(Inventory().Breaks())+" | Supplies: "+std::to_string(Inventory().Supplies())
        +" | Pickups: "+std::to_string(Inventory().Pickups())+" | Drops: "+std::to_string(Inventory().Drops().size());
    if (!turretMode) return std::string("AEGIS RANGE | ")+(paused ? "PAUSED | " : "")
        +"Targets: "+std::to_string(range.RemainingTargets())+"/3 | Shots: "+std::to_string(range.Shots())
        +" | Blocked: "+std::to_string(range.BlockedShots())+" | Cover: "+(range.CoverEnabled()?"ON":"OFF")
        +weaponStatus+" | G drop / E pickup or supply / T turret / R reset";
    const char* state=encounter.Result()==combat::EncounterResult::Failed ? "FAILED"
        : encounter.Result()==combat::EncounterResult::Cleared ? "CLEARED"
        : encounter.Result()==combat::EncounterResult::Securing ? "SECURING"
        : encounter.Phase()==combat::TurretPhase::Warning ? "WARNING"
        : encounter.Phase()==combat::TurretPhase::Cooldown ? "COOLDOWN"
        : encounter.Phase()==combat::TurretPhase::Disarmed ? "DISARMED" : IsDroneMode() ? "PATROL" : "READY";
    return std::string(IsDroneMode() ? "AEGIS DRONE | " : corridorMode ? "AEGIS CORRIDOR | " : "AEGIS TURRET | ")+(paused ? "PAUSED | " : "")+state
        +" | HP: "+std::to_string(encounter.Health())+" | Hits: "+std::to_string(encounter.HitsTaken())
        +" | EnemyShots: "+std::to_string(encounter.EnemyShots())+" | PlayerShots: "+std::to_string(encounter.PlayerShots())
        +" | Hostiles: "+std::to_string(encounter.EnemyFlights().size())+" | Cover: "+(encounter.CoverEnabled()?"ON":"OFF")+" | Analysis: "+(CanAnalyze()?"READY":"LOCKED")+" | Entries: "+std::to_string(analysis.Entries())+" | Exits: "+std::to_string(analysis.Exits())+" | Door: "+std::to_string(static_cast<int>(DoorProgress()*100))+weaponStatus+(corridorMode ? " | F3 enemy / G drop / E use / R retry / F2 range" : " | G drop / E pickup or supply / R retry / T range");
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
