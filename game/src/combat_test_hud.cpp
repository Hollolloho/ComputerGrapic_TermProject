#include "combat_test_scene.h"

#include <array>
#include <string>

namespace aegis {
namespace {

// Minimal built-in debug labels: no font package or runtime asset dependency.
std::array<unsigned char,7> Glyph(char letter)
{
    switch(letter) {
    case 'A': return {14,17,17,31,17,17,17}; case 'B': return {30,17,17,30,17,17,30};
    case 'C': return {14,17,16,16,16,17,14}; case 'D': return {30,17,17,17,17,17,30};
    case 'E': return {31,16,16,30,16,16,31}; case 'F': return {31,16,16,30,16,16,16};
    case 'G': return {14,17,16,23,17,17,14}; case 'H': return {17,17,17,31,17,17,17};
    case 'I': return {31,4,4,4,4,4,31}; case 'J': return {7,2,2,2,18,18,12};
    case 'K': return {17,18,20,24,20,18,17}; case 'L': return {16,16,16,16,16,16,31};
    case 'M': return {17,27,21,21,17,17,17}; case 'N': return {17,25,21,19,17,17,17};
    case 'O': return {14,17,17,17,17,17,14}; case 'P': return {30,17,17,30,16,16,16};
    case 'Q': return {14,17,17,17,21,18,13}; case 'R': return {30,17,17,30,20,18,17};
    case 'S': return {15,16,16,14,1,1,30}; case 'T': return {31,4,4,4,4,4,4};
    case 'U': return {17,17,17,17,17,17,14}; case 'V': return {17,17,17,17,17,10,4};
    case 'W': return {17,17,17,21,21,21,10}; case 'X': return {17,17,10,4,10,17,17};
    case 'Y': return {17,17,10,4,4,4,4}; case 'Z': return {31,1,2,4,8,16,31};
    case '0': return {14,17,19,21,25,17,14}; case '1': return {4,12,4,4,4,4,14};
    case '2': return {14,17,1,2,4,8,31}; case '3': return {30,1,1,14,1,1,30};
    case '4': return {2,6,10,18,31,2,2}; case '5': return {31,16,16,30,1,1,30};
    case '6': return {14,16,16,30,17,17,14}; case '7': return {31,1,2,4,8,8,8};
    case '8': return {14,17,17,14,17,17,14}; case '9': return {14,17,17,15,1,1,14};
    case '/': return {1,2,2,4,8,8,16}; default: return {};
    }
}

void Label(const std::string& text,int x,int y,int scale,const glm::vec3& color)
{
    glClearColor(color.r,color.g,color.b,1);
    for(char letter:text) {
        const auto glyph=Glyph(letter);
        for(int row=0;row<7;++row) for(int column=0;column<5;++column) {
            if (glyph[row] & (1 << (4-column))) {
                glScissor(x+column*scale,y+(6-row)*scale,scale,scale);
                glClear(GL_COLOR_BUFFER_BIT);
            }
        }
        x+=6*scale;
    }
}

}

void CombatTestScene::DrawHud(int width,int height) const
{
    const int scale=width<900 ? 2 : 3;
    if (AnalysisActive()) {
        Label("DRONE ANALYSIS",24,height-40,scale,{0.3f,1,0.9f});
        Label("S07 / SENSOR / ROTORS / WEAPON",24,height-80,2,{0.8f,0.9f,1});
        Label("E OR ESC RETURN",24,24,2,{0.3f,1,0.8f});
        return;
    }
    if (TerminalTarget()) Label(CanAnalyze() ? "E ANALYZE DRONE" : "ANALYSIS LOCKED / AREA NOT SAFE",
        width/2-180,height/2-80,2,CanAnalyze()?glm::vec3(0.3f,1,0.8f):glm::vec3(1,0.45f,0.2f));
    if (corridorMode) Label(IsDroneMode() ? "DRONE COMBAT / F3 TURRET / F2 RANGE" : "CORRIDOR COMBAT / F3 DRONE / F2 RANGE",24,height-135,2,{0.3f,0.9f,1});
    if (CanAnalyze()) Label("SECTOR SAFE / ANALYSIS READY",24,height-165,2,{0.2f,1,0.6f});
    const auto armed=Inventory().Armed();
    Label(armed ? "CW01C READY / G DROP" : "EMPTY / LOOK LEFT DOWN FOR SUPPLY",24,height-105,2,
        armed ? glm::vec3(0.2f,1,0.7f) : glm::vec3(1,0.7f,0.2f));
    const auto hint=InteractionHint();
    if (hint!=combat::WeaponInteraction::None) {
        const std::string text=hint==combat::WeaponInteraction::Supply ? "E RESUPPLY" : "E PICK UP";
        Label(text,(width-static_cast<int>(text.size())*12)/2,height/2-50,2,{0.3f,1,0.8f});
    }
    if (!turretMode) {
        Label("SHOOTING RANGE",24,height-40,scale,{0.2f,1,0.7f});
        Label("LMB FIRE  RMB AIM  E USE  G DROP  B COVER  T TURRET  R RESET",24,24,2,{0.8f,0.9f,1});
        return;
    }
    const auto result=encounter.Result();
    const glm::vec3 hpColor=encounter.Health()>25 ? glm::vec3(0.1f,0.9f,0.6f) : glm::vec3(1,0.2f,0.1f);
    Label("HP "+std::to_string(encounter.Health())+"/100",24,height-40,scale,hpColor);
    glClearColor(0.15f,0.2f,0.26f,1); glScissor(24,height-72,240,16); glClear(GL_COLOR_BUFFER_BIT);
    glClearColor(hpColor.r,hpColor.g,hpColor.b,1);
    glScissor(24,height-72,240*encounter.Health()/100,16); glClear(GL_COLOR_BUFFER_BIT);
    if (result==combat::EncounterResult::Running || result==combat::EncounterResult::Securing) {
        std::string phaseLabel=IsDroneMode() ? "PATROL" : "READY";
        if (encounter.Phase()==combat::TurretPhase::Warning) phaseLabel="WARNING";
        if (encounter.Phase()==combat::TurretPhase::Cooldown) phaseLabel="COOLDOWN";
        if (encounter.Phase()==combat::TurretPhase::Disarmed) phaseLabel="DISARMED";
        if (result==combat::EncounterResult::Securing) phaseLabel="INCOMING SHOTS / STAY ALERT";
        Label(phaseLabel,(width-static_cast<int>(phaseLabel.size())*6*scale)/2,height-40,scale,{1,0.45f,0.2f});
    } else if (result==combat::EncounterResult::Cleared) {
        Label("SECTOR CLEAR",(width-12*6*scale)/2,height-70,scale,{0.2f,1,0.65f});
        Label("R RESTART",(width-9*12)/2,height-95,2,{1,1,1});
    } else {
        glClearColor(0.035f,0.05f,0.075f,1);
        glScissor(width/2-220,height/2,440,145);
        glClear(GL_COLOR_BUFFER_BIT);
        const std::string label=result==combat::EncounterResult::Failed ? "FAILED" : "CLEAR";
        const auto color=result==combat::EncounterResult::Failed ? glm::vec3(1,0.25f,0.2f) : glm::vec3(0.2f,1,0.65f);
        const int bigScale=scale*2;
        Label(label,(width-static_cast<int>(label.size())*6*bigScale)/2,height/2+60,bigScale,color);
        Label("R RETRY",(width-7*6*scale)/2,height/2+20,scale,{1,1,1});
    }
    if (encounter.DamageFlash()>0) {
        glClearColor(0.8f,0.04f,0.07f,1);
        glScissor(0,0,width,6); glClear(GL_COLOR_BUFFER_BIT);
        glScissor(0,height-6,width,6); glClear(GL_COLOR_BUFFER_BIT);
        glScissor(0,0,6,height); glClear(GL_COLOR_BUFFER_BIT);
        glScissor(width-6,0,6,height); glClear(GL_COLOR_BUFFER_BIT);
    }
    Label(corridorMode ? "LMB FIRE  RMB AIM  E USE  G DROP  R RESET  F2 RANGE" : "LMB FIRE  RMB AIM  E USE  G DROP  B COVER  T RANGE  R RESET",24,24,2,{0.8f,0.9f,1});
}

} // namespace aegis
