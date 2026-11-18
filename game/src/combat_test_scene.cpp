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
#include <limits>

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
uniform float opacity;
out vec4 outColor;
void main() {
    vec3 normal=normalize(cross(dFdx(localPosition),dFdy(localPosition)));
    float light=0.65+0.35*abs(dot(normal,normalize(vec3(0.4,0.8,0.5))));
    outColor=vec4(color*light,opacity);
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
    opacityLocation = glGetUniformLocation(program, "opacity");
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
    if (!turretView.Init(projectDirectory) || !weapon.Init(projectDirectory) || !droneRenderer.Init(projectDirectory/"game/shaders") || !drone.Init(projectDirectory) || !InitTerminal(projectDirectory) || !bossView.Init(projectDirectory)) { Clear(); return false; }
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

bool CombatTestScene::SetPlayerView(const PlayerView& view) {
    if(analysis.Active()) return false;
    if(!externalView.Set(view)) return false;
    lookHeld=false; return true;
}
PlayerView CombatTestScene::CurrentPlayerView() const {
    const PlayerView fallback{camera.EyePosition(),camera.ViewDirection(),{0,1,0},60};
    return externalView.Selected(fallback);
}

void CombatTestScene::Reset()
{
    analysis.Reset(); ClearPlayerView(); feedback.Reset(); time.Reset(); slowPreview=false; forceNormal=false;
    movementTimeAvailable=manualNormalTime=false; movementScale=0.05;
    range.Reset(); encounter.Reset(); weapon.Reset(); drone.Reset(); boss.Reset(); bossCovered=false; yaw = pitch = 0;
    if (bossMode) { const auto d=glm::normalize(boss.CoreCenter()-(CameraRoot()+glm::vec3(0,1.65f,0))); pitch=glm::degrees(std::asin(d.y)); }
    camera.SetPose(CameraRoot(), yaw, pitch);
    // Held input remains latched until release (R must not create a fresh shot).
    lookHeld = false;
}

void CombatTestScene::SetCorridorMode(bool enabled) {
    bossMode=false; combatMapMode=false; encounter.SetSupportTurret(false);
    encounter.SetDroneSpawn({-0.65f,1.8f,-10},0,0);
    encounter.SetSupply(combat::WeaponInventory::SupplyPosition());
    corridorMode=enabled;
    std::vector<combat::Collider> walls;
    if (enabled) { walls.assign(corridor_asset::walls.begin(),corridor_asset::walls.end()); walls.push_back(terminal_asset::collider); }
    encounter.SetEnvironment(walls);
    encounter.SetExitDoors(enabled ? 10010:0,enabled ? 10011:0,enabled ? corridor_asset::doorTravel:0);
    encounter.SetDroneMode(enabled);
    SetTurretMode(enabled);
}

void CombatTestScene::SetCombatMapMode(bool enabled) {
    if(!enabled) { SetCorridorMode(true); return; }
    combatMapMode=true; corridorMode=turretMode=true; bossMode=false;
    std::vector<combat::Collider> walls(combat_map_asset::walls.begin(),combat_map_asset::walls.end());
    walls.push_back(TerminalCollider());
    encounter.SetEnvironment(walls); encounter.SetExitDoors(0,0,0);
    encounter.SetDroneSpawn(combat_map_asset::droneSpawn,glm::radians(combat_map_asset::droneYawDegrees),combat_map_asset::droneSpawn.x);
    encounter.SetSupply(combat_map_asset::supply); encounter.SetDroneMode(true); encounter.SetSupportTurret(true);
    Reset(); smokeMapSlow=smokeMapCoverBlocked=smokeFailureSeen=smokeMapBoth=smokeMapTurretShot=smokeMapLocked=false;
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

glm::vec3 CombatTestScene::CameraRoot() const { return bossMode ? (bossCovered ? combat::BossEncounter::CoverRoot():combat::BossEncounter::FiringRoot()):combatMapMode ? combat_map_asset::playerStart:glm::vec3(0); }
void CombatTestScene::SetBossMode(bool enabled) {
    bossMode=enabled; if(enabled) combatMapMode=false; Reset(); smokeBossFailureSeen=false;
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
    if(!time.CanFire()) return false;
    const bool fired=bossMode ? boss.Fire(PlayerEye(),direction):turretMode ? encounter.Fire(PlayerEye(),direction)
        : range.Fire(PlayerEye(),direction);
    if (fired) { time.RequestShot(); weapon.OnShot(); }
    return fired;
}

const combat::WeaponInventory& CombatTestScene::Inventory() const { return bossMode ? boss.Inventory():turretMode ? encounter.Inventory() : range.Inventory(); }
bool CombatTestScene::Interact(const glm::vec3& direction) {
    if (bossMode) return boss.Interact(PlayerEye(),direction);
    if (TerminalTarget(direction)) return analysis.Open(CanAnalyze(),TerminalCollider(),PlayerEye(),direction,encounter.Colliders());
    return turretMode ? encounter.Interact(PlayerEye(),direction) : range.Interact(PlayerEye(),direction);
}
glm::vec3 CombatTestScene::TerminalPosition() const { return combatMapMode ? combat_map_asset::terminal:terminal_asset::position; }
combat::Collider CombatTestScene::TerminalCollider() const {
    auto box=terminal_asset::collider;
    const auto offset=TerminalPosition()-terminal_asset::position;
    box.minimum+=offset; box.maximum+=offset; return box;
}
glm::vec3 CombatTestScene::SupplyPosition() const { return combatMapMode ? combat_map_asset::supply:combat::WeaponInventory::SupplyPosition(); }
std::vector<combat::Collider> CombatTestScene::PlayerObstacles() const {
    if(bossMode) {
        std::vector<combat::Collider> walls(boss_asset::walls.begin(),boss_asset::walls.end());
        glm::vec3 low(std::numeric_limits<float>::max()),high(std::numeric_limits<float>::lowest());
        for(int i=0;i<boss_asset::partCount;++i) {
            const auto& part=boss_asset::parts[i];
            for(int corner=0;corner<8;++corner) {
                const auto point=glm::vec3(boss.Pose().parts[i]*glm::vec4(corner&1 ? part.high.x:part.low.x,
                    corner&2 ? part.high.y:part.low.y,corner&4 ? part.high.z:part.low.z,1));
                low=glm::min(low,point); high=glm::max(high,point);
            }
        }
        walls.push_back({29000,combat::HitKind::Armor,low,high}); return walls;
    }
    if(combatMapMode) {
        std::vector<combat::Collider> walls(combat_map_asset::playerWalls.begin(),combat_map_asset::playerWalls.end());
        walls.push_back(TerminalCollider());
        if(encounter.HasSupportTurret() && encounter.SupportTurret().Alive()) {
            auto body=encounter.SupportTurret().Body(); body.kind=combat::HitKind::Armor; walls.push_back(body);
        }
        return walls;
    }
    return turretMode ? encounter.Colliders():std::vector<combat::Collider>(range.Colliders().begin(),range.Colliders().end());
}
bool CombatTestScene::TerminalTarget() const { return TerminalTarget(PlayerForward()); }
bool CombatTestScene::TerminalTarget(const glm::vec3& direction) const {
    if (!corridorMode || bossMode) return false;
    return analysis.IsFirstTarget(TerminalCollider(),PlayerEye(),direction,encounter.Colliders(),
        Inventory().InteractionDistance(PlayerEye(),direction,encounter.Colliders()));
}
combat::WeaponInteraction CombatTestScene::InteractionHint() const {
    if (bossMode) return boss.InteractionHint(PlayerEye(),PlayerForward());
    if (TerminalTarget()) return combat::WeaponInteraction::None;
    return turretMode ? encounter.InteractionHint(PlayerEye(),PlayerForward())
        : range.InteractionHint(PlayerEye(),PlayerForward());
}

void CombatTestScene::SampleInput(GLFWwindow* window, bool active)
{
    const bool preview=glfwGetKey(window,GLFW_KEY_F5)==GLFW_PRESS;
    forceNormal=active && !analysis.Active() && (glfwGetKey(window,GLFW_KEY_LEFT_SHIFT)==GLFW_PRESS
        || glfwGetKey(window,GLFW_KEY_RIGHT_SHIFT)==GLFW_PRESS);
    if(active && !analysis.Active() && preview && !previewHeld) {
        if(movementTimeAvailable) manualNormalTime=!manualNormalTime;
        else slowPreview=!slowPreview;
    }
    previewHeld=preview;
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
        if (debugControls && bossToggle && !bossToggleHeld) SetBossMode(!bossMode);
        if (debugControls && !bossMode && corridorMode && droneToggle && !droneToggleHeld) SetDroneMode(!IsDroneMode());
        if (debugControls && !bossMode && !corridorMode && turret && !turretHeld) SetTurretMode(!turretMode);
        const bool playing=bossMode ? boss.Result()==combat::EncounterResult::Running || boss.Result()==combat::EncounterResult::Securing : !turretMode || encounter.Result()==combat::EncounterResult::Running || encounter.Result()==combat::EncounterResult::Securing;
        const bool canEquip=bossMode ? boss.Result()!=combat::EncounterResult::Failed : !turretMode || encounter.Result()!=combat::EncounterResult::Failed;
        if (!HasExternalPlayerView() && canEquip && right && lookHeld) {
            yaw -= static_cast<float>(x-lastX)*0.14f;
            pitch = std::clamp(pitch-static_cast<float>(y-lastY)*0.14f, -65.0f,65.0f);
            camera.SetPose(CameraRoot(), yaw,pitch);
        }
        if (!HasExternalPlayerView() && playing && cover && !coverHeld) {
            if (bossMode) ToggleBossCover();
            else if (turretMode) encounter.ToggleCover(); else range.ToggleCover();
        }
        if (canEquip && drop && !dropHeld) {
            if (bossMode) boss.DropWeapon(PlayerEye(),PlayerForward());
            else if (turretMode) encounter.DropWeapon(PlayerEye(),PlayerForward());
            else range.DropWeapon(PlayerEye(),PlayerForward());
        }
        if (canEquip && interact && !interactHeld) Interact(PlayerForward());
        if (playing && left && !mouseHeld) {
            Fire(PlayerForward());
        }
    }
    mouseHeld = left; coverHeld = cover; lookHeld = active && right;
    turretHeld=turret; droneToggleHeld=droneToggle; bossToggleHeld=bossToggle; dropHeld=drop; interactHeld=interact;
    lastX=x; lastY=y;
}

void CombatTestScene::Update(double realDt, double gameDt)
{
    if (analysis.Active()) return;
    const auto frame=time.Advance(realDt,gameDt,forceNormal);
    realDt=frame.realDt; gameDt=frame.gameDt;
    if(realDt<=0) return;
    if (bossMode) boss.Update(static_cast<float>(realDt),static_cast<float>(gameDt),PlayerEye(),PlayerForward());
    else if (turretMode) encounter.Update(static_cast<float>(realDt),static_cast<float>(gameDt),PlayerEye(),PlayerForward());
    else range.Update(static_cast<float>(realDt), static_cast<float>(gameDt));
    if (IsDroneMode() && encounter.Result()==combat::EncounterResult::Running) drone.UpdateCombat(gameDt);
    weapon.Update(realDt);
    feedback.Update(static_cast<float>(gameDt));
    const auto forward=PlayerForward();
    const auto right=glm::normalize(glm::cross(forward,CurrentPlayerView().up));
    const auto origin=PlayerEye()+forward*0.8f+right*0.22f+glm::vec3(0,-0.14f,0);
    feedback.Observe(bossMode ? boss.Health():turretMode ? encounter.Health():100,
        Inventory().Breaks(),origin,right,forward);
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
    const std::function<void(const glm::mat4&,const glm::vec3&)>& drawEnvironment,bool showHud) const
{
    if (width <= 0 || height <= 0) return;
    if (analysis.Active()) { DrawAnalysis(width,height); return; }
    glViewport(0,0,width,height); glDisable(GL_FRAMEBUFFER_SRGB); glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE); glEnable(GL_DEPTH_TEST); glDepthMask(GL_TRUE);
    glClearColor(0.025f,0.04f,0.065f,1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    const auto projection=glm::perspective(glm::radians(CurrentPlayerView().verticalFovDegrees),static_cast<float>(width)/height,0.05f,80.0f);
    const auto vp=projection*PlayerViewMatrix();
    if (bossMode) bossView.Draw(projection,PlayerViewMatrix(),PlayerEye(),boss);
    else if (drawEnvironment) drawEnvironment(vp,PlayerEye());
    if (corridorMode && !bossMode) {
        droneRenderer.Begin(PlayerEye(),{0.45f,0.9f,0.7f},{1,1,1},3,0.3f);
        droneRenderer.Draw(terminalMeshes,vp,glm::translate(glm::mat4(1),TerminalPosition()));
    }
    if (IsDroneMode() && encounter.EnemyAlive()) {
        const auto& state=encounter.DroneState();
        const auto world=glm::rotate(glm::translate(glm::mat4(1),state.Position()),state.Yaw(),glm::vec3(0,1,0));
        droneRenderer.Begin(PlayerEye(),{0.45f,0.9f,0.7f},{1,1,1},4,0.25f);
        drone.Draw(droneRenderer,vp,world,state.Phase()==combat::DronePhase::Warning,state.Armed());
    }
    if(!bossMode && encounter.HasSupportTurret()) turretView.Draw(vp,encounter.SupportTurret());
    glUseProgram(program); glUniform1f(opacityLocation,1); glBindVertexArray(vao);
    const std::span<const combat::Collider> boxes=bossMode ? std::span<const combat::Collider>() : turretMode
        ? std::span<const combat::Collider>(encounter.Colliders())
        : std::span<const combat::Collider>(range.Colliders());
    for (const auto& box : boxes) {
        if ((encounter.HasSupportTurret() && (box.id==combat::FixedTurret::BodyId || box.id==combat::FixedTurret::WeaponId)) || (corridorMode && box.id==12000) || (IsDroneMode() && (box.id==500 || box.id==501)) || !box.enabled || (corridorMode && box.kind==combat::HitKind::Wall)) continue;
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
    for(const auto& shard:feedback.Shards()) {
        const float scale=shard.size*(1-shard.age/shard.lifetime);
        const auto transform=vp*glm::rotate(glm::translate(glm::mat4(1),shard.position),
            shard.size*93+shard.age*9,glm::normalize(glm::vec3(1,0.6f,0.3f)))*glm::scale(glm::mat4(1),glm::vec3(scale*2,scale,scale));
        glUniformMatrix4fv(transformLocation,1,GL_FALSE,glm::value_ptr(transform));
        const glm::vec3 color=shard.size<0.017f ? glm::vec3(0.65f,0.69f,0.74f)
            :shard.size<0.022f ? glm::vec3(0.76f,0.66f,0.48f):glm::vec3(0.23f,0.28f,0.33f);
        glUniform3fv(colorLocation,1,glm::value_ptr(color)); glDrawArrays(GL_TRIANGLES,0,36);
    }
    const auto supply=bossMode ? boss_asset::supplyWeapon:SupplyPosition();
    if (!bossMode) DrawBox(vp,supply+glm::vec3(-0.32f,-0.5f,-0.27f),supply+glm::vec3(0.32f,-0.15f,0.27f),{0.1f,0.55f,0.38f});
    glBindVertexArray(0); glUseProgram(0);
    weapon.DrawWorld(projection,PlayerViewMatrix(),supply,bossMode ? 0.0f:90.0f);
    for (const auto& drop:Inventory().Drops()) weapon.DrawWorld(projection,PlayerViewMatrix(),drop.position);
    if (Inventory().Armed()) weapon.Draw(static_cast<float>(width)/height);
    // Crisp centre crosshair drawn in framebuffer pixels.
    glEnable(GL_SCISSOR_TEST);
    glClearColor(range.RemainingTargets()==0 ? 0.15f : 0.95f,1,0.7f,1);
    glScissor(width/2-8,height/2-1,17,3); glClear(GL_COLOR_BUFFER_BIT);
    glScissor(width/2-1,height/2-8,3,17); glClear(GL_COLOR_BUFFER_BIT);
    if (showHud) DrawHud(width,height);
    glDisable(GL_SCISSOR_TEST);
    DrawDamageFeedback(width,height);
}

void CombatTestScene::DrawDamageFeedback(int width,int height) const {
    if(feedback.DamageStrength()<=0 || width<=0 || height<=0) return;
    glDisable(GL_DEPTH_TEST); glDepthMask(GL_FALSE); glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    glUseProgram(program); glBindVertexArray(vao);
    const glm::vec3 red(1,0.025f,0.045f); glUniform3fv(colorLocation,1,glm::value_ptr(red));
    const auto rectangle=[&](float x,float y,float w,float h) {
        const auto transform=glm::scale(glm::translate(glm::mat4(1),glm::vec3(x,y,0)),glm::vec3(w,h,1));
        glUniformMatrix4fv(transformLocation,1,GL_FALSE,glm::value_ptr(transform));
        glDrawArrays(GL_TRIANGLES,0,6);
    };
    for(int band=0;band<6;++band) {
        const float inset=static_cast<float>(band)*0.018f,span=2-2*inset;
        glUniform1f(opacityLocation,feedback.DamageStrength()*(0.7f-static_cast<float>(band)*0.1f));
        rectangle(-1+inset+0.009f,0,0.018f,span);
        rectangle(1-inset-0.009f,0,0.018f,span);
        rectangle(0,-1+inset+0.009f,span-0.036f,0.018f);
        rectangle(0,1-inset-0.009f,span-0.036f,0.018f);
    }
    glUniform1f(opacityLocation,1); glBindVertexArray(0); glUseProgram(0);
    glDisable(GL_BLEND); glDepthMask(GL_TRUE); glEnable(GL_DEPTH_TEST);
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
    analysis.Reset(); ClearPlayerView(); feedback.Reset(); bossView.Clear();
    for (auto& mesh:terminalMeshes) mesh.Clear();
    terminalMeshes.clear();
    turretView.Clear(); drone.Clear(); droneRenderer.Clear();
    weapon.Clear();
    if (vbo) glDeleteBuffers(1,&vbo);
    if (vao) glDeleteVertexArrays(1,&vao);
    if (program) glDeleteProgram(program);
    vbo=vao=program=0;
}

void CombatTestScene::PlayerViewSmokeFrame(int frame) {
    const auto aim=[&](const glm::vec3& eye,const glm::vec3& target,float fov=45) {
        return SetPlayerView({eye,target-eye,{0,1,0},fov});
    };
    if(frame==1) encounter.SetSupportTurret(false); // Isolate camera/interaction contracts from encounter composition.
    if(frame==1) aim(combat_map_asset::playerStart+glm::vec3(1,1.65f,-1),combat_map_asset::droneSpawn);
    if(frame==5) Fire(PlayerForward());
    if(frame==80) {
        smokeViewShot=HasExternalPlayerView() && encounter.Result()==combat::EncounterResult::Cleared
            && encounter.PlayerShots()==1 && Inventory().Breaks()==1;
        aim({0,1.65f,2},SupplyPosition());
        const bool far=!Interact(PlayerForward());
        aim({-1.2f,1.65f,-2.4f},SupplyPosition());
        smokeViewSupply=far && Interact(PlayerForward()) && Inventory().Armed();
        encounter.DropWeapon(PlayerEye(),{1,0,0});
        const auto drop=Inventory().Drops().back().position;
        aim(drop+glm::vec3(0,1.65f,5),drop);
        const bool farDrop=!Interact(PlayerForward());
        aim({-1.2f,1.65f,-2.4f},drop);
        smokeViewPickup=farDrop && Interact(PlayerForward()) && Inventory().Pickups()==1;
    }
    if(frame==100) {
        const auto target=terminal_asset::interactionPoint+TerminalPosition()-terminal_asset::position;
        aim({0.1f,1.65f,-2},target,40);
        smokeViewSnapshot=CurrentPlayerView();
        const bool opened=Interact(PlayerForward());
        const bool rejected=!SetPlayerView({{99,99,99},{0,0,-1},{0,1,0},60});
        ClearPlayerView();
        smokeViewAnalysis=opened && AnalysisActive() && rejected && HasExternalPlayerView();
    }
    if(frame==120) {
        analysis.Close();
        smokeViewRestored=CurrentPlayerView().eye==smokeViewSnapshot.eye
            && CurrentPlayerView().forward==smokeViewSnapshot.forward
            && CurrentPlayerView().verticalFovDegrees==smokeViewSnapshot.verticalFovDegrees;
    }
    if(frame==130) {
        Reset(); smokeViewReset=!HasExternalPlayerView() && encounter.Health()==100
            && CurrentPlayerView().eye==combat_map_asset::playerStart+glm::vec3(0,1.65f,0);
    }
    if(frame==135) aim({2,1.65f,-4},combat_map_asset::droneSpawn);
    if(frame==300) smokeViewAim=encounter.EnemyShots()==1
        && glm::length(encounter.DroneState().AimPoint()-glm::vec3(2,1.65f,-4))<0.001f;
    if(frame==310) aim({-0.5f,1.65f,-4},encounter.DroneState().Position());
    if(frame==400) smokeViewDodge=encounter.EnemyShots()==1 && encounter.Health()==100;
    if(frame==410) { Reset(); smokeViewDone=!HasExternalPlayerView(); }
}
bool CombatTestScene::PlayerViewSmokePassed() const {
    std::cout<<"External view checks: shot="<<smokeViewShot<<" supply="<<smokeViewSupply
        <<" pickup="<<smokeViewPickup<<" analysis="<<smokeViewAnalysis<<" restore="<<smokeViewRestored
        <<" reset="<<smokeViewReset<<" aim="<<smokeViewAim<<" dodge="<<smokeViewDodge<<" done="<<smokeViewDone<<'\n';
    return smokeViewShot && smokeViewSupply && smokeViewPickup && smokeViewAnalysis && smokeViewRestored
        && smokeViewReset && smokeViewAim && smokeViewDodge && smokeViewDone;
}

void CombatTestScene::CampaignSmokeFrame(int frame) {
    if (bossMode) {
        if (frame==1) { Fire(boss.CoreCenter()-PlayerEye()); ToggleBossCover(); }
        if (boss.Result()!=combat::EncounterResult::Running) return;
        if (boss.Phase()==combat::BossPhase::Exposed && Inventory().Armed()) {
            if (bossCovered) ToggleBossCover();
            if (boss.PlayerFlights().empty()) Fire(boss.CoreCenter()-PlayerEye());
        } else {
            if (!bossCovered) ToggleBossCover();
            if (!Inventory().Armed()) Interact(boss_asset::supplyInteraction-PlayerEye());
        }
    } else {
        if(frame==1) Fire(encounter.DroneState().Position()-PlayerEye());
        if(!encounter.EnemyAlive() && encounter.RemainingEnemies()>0 && encounter.PlayerFlights().empty()) {
            if(!Inventory().Armed()) Interact(SupplyPosition()-PlayerEye());
            if(Inventory().Armed()) Fire(encounter.SupportTurret().Position()+glm::vec3(0,0.9f,0)-PlayerEye());
        }
    }
}

void CombatTestScene::SetSmokeFrame(int frame)
{
    if(combatMapMode) {
        smokeMapBoth=smokeMapBoth || encounter.RemainingEnemies()==2;
        smokeMapTurretShot=smokeMapTurretShot || std::any_of(encounter.EnemyFlights().begin(),encounter.EnemyFlights().end(),[](const combat::Flight& f){ return f.projectile.sourceWeaponId==combat::FixedTurret::ShotId; });
        if(frame==1) slowPreview=true;
        if(frame==180) { smokeMapSlow=encounter.Health()==100 && encounter.EnemyShots()==0
            && time.Scale()<0.051 && glm::length(encounter.DroneState().Position()-combat_map_asset::droneSpawn)<0.2f; slowPreview=false; }
        if(frame==1200) { smokeFailureSeen=encounter.Result()==combat::EncounterResult::Failed; Reset(); }
        if(frame==1201) Fire(glm::vec3(-1.9f,1,-7.45f)-PlayerEye());
        if(frame==1239) smokeMapCoverBlocked=encounter.EnemyAlive() && Inventory().Armed() && encounter.PlayerFlights().empty();
        if(frame==1240) Fire(encounter.DroneState().Position()-PlayerEye());
        if(frame==1310) smokeMapLocked=!encounter.EnemyAlive() && encounter.RemainingEnemies()==1 && !CanAnalyze() && encounter.Result()==combat::EncounterResult::Running;
        if(frame==1320) Interact(SupplyPosition()-PlayerEye());
        if(frame==1321) Fire(encounter.SupportTurret().Position()+glm::vec3(0,0.9f,0)-PlayerEye());
        if(frame==1450) Interact(SupplyPosition()-PlayerEye());
        return;
    }
    if (bossMode) {
        if (frame==900) { smokeBossFailureSeen=boss.Result()==combat::EncounterResult::Failed; Reset(); }
        if (frame==901) { Fire(boss.CoreCenter()-PlayerEye()); ToggleBossCover(); }
        if (frame>901 && boss.Result()==combat::EncounterResult::Running) {
            if (boss.Phase()==combat::BossPhase::Exposed && Inventory().Armed()) {
                if (bossCovered) ToggleBossCover();
                if (boss.PlayerFlights().empty()) Fire(boss.CoreCenter()-PlayerEye());
            } else {
                if (!bossCovered) ToggleBossCover();
                if (!Inventory().Armed()) Interact(boss_asset::supplyInteraction-PlayerEye());
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
        if (frame==1150) { smokeDoorSnapshot=DoorProgress(); Interact(terminal_asset::interactionPoint-PlayerEye()); }
        if (frame==1160) { smokeAnalysisSeen=analysis.Active(); smokeWorldFrozen=DoorProgress()==smokeDoorSnapshot && smokeDoorSnapshot<1; }
        if (frame==1190) analysis.Close();
        if (frame==1191) smokeAnalysisReturned=!analysis.Active() && analysis.Entries()>0 && analysis.Exits()>0;
        if (frame==1060) Fire(encounter.DroneState().Position()-PlayerEye());
        return;
    }
    if (turretMode) {
        if (frame==660) {
            smokeFailureSeen=encounter.Result()==combat::EncounterResult::Failed;
            Reset();
        }
        if (frame==661) Fire(PlayerForward());
        return;
    }
    if (frame==1) Fire({0,0,-1});
    if (frame==45) range.DropWeapon(PlayerEye(),PlayerForward());
    if (frame==50 && !range.Inventory().Drops().empty()) Interact(range.Inventory().Drops()[0].position-PlayerEye());
    if (frame==70) { range.ToggleCover(); Fire({0,0,-1}); }
    if (frame==130 || frame==200) Interact(combat::WeaponInventory::SupplyPosition()-PlayerEye());
    if (frame==140) Fire(glm::vec3(-2.1f,1.6f,-9.8f)-PlayerEye());
    if (frame==210) Fire(glm::vec3(2.1f,1.6f,-9.8f)-PlayerEye());
}

bool CombatTestScene::SmokePassed() const
{
    if(feedback.BreakBursts()!=Inventory().Breaks()) return false;
    if(combatMapMode) {
        std::cout<<"Mixed map checks: both="<<smokeMapBoth<<" turretShot="<<smokeMapTurretShot<<" locked="<<smokeMapLocked<<" slow="<<smokeMapSlow<<" cover="<<smokeMapCoverBlocked<<" failure="<<smokeFailureSeen<<" remaining="<<encounter.RemainingEnemies()<<" hp="<<encounter.Health()<<" breaks="<<Inventory().Breaks()<<'\n';
        return smokeMapBoth && smokeMapTurretShot && smokeMapLocked && smokeMapSlow && smokeMapCoverBlocked && smokeFailureSeen
            && encounter.Result()==combat::EncounterResult::Cleared && encounter.Health()>0
            && encounter.RemainingEnemies()==0 && encounter.PlayerShots()==3 && Inventory().Armed()
            && Inventory().Breaks()==2 && Inventory().Supplies()==2 && encounter.EnemyFlights().empty();
    }
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
        +" | Enemies: "+std::to_string(encounter.RemainingEnemies())
        +" | Turret: "+(encounter.HasSupportTurret() ? !encounter.SupportTurret().Alive() ? "DESTROYED":encounter.SupportTurret().Phase()==combat::TurretPhase::Warning ? "WARNING":!encounter.SupportTurret().Armed() ? "DISARMED":"ACTIVE":"OFF")
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
