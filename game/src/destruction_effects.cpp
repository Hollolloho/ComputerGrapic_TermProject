#include "destruction_effects.h"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>
namespace aegis::combat {
void DestructionEffects::Reset() { debris={};known={};wasAlive={};bursts=deaths=0;deathAge=0; }
void DestructionEffects::ObserveEnemy(int slot,bool alive,const glm::mat4& world) {
    if(slot<0 || slot>=static_cast<int>(debris.size())) return;
    if(known[slot] && wasAlive[slot] && !alive) { debris[slot]={world,0,true};++bursts; }
    known[slot]=true;wasAlive[slot]=alive;
}
void DestructionEffects::UpdateWorld(float gameDt) {
    if(!std::isfinite(gameDt) || gameDt<=0) return;
    for(auto& item:debris) if(item.active) { item.age=std::min(DebrisLifetime,item.age+gameDt);item.active=item.age<DebrisLifetime; }
}
void DestructionEffects::ObserveHealth(int health) { if(health<=0 && !DeathStarted()) { deaths=1;deathAge=0; } }
void DestructionEffects::UpdateDeath(float realDt) {
    if(DeathPlaying() && std::isfinite(realDt) && realDt>0) deathAge=std::min(DeathDuration,deathAge+realDt);
}
glm::mat4 DestructionEffects::PieceTransform(const glm::mat4& world,int piece,float age) {
    if(!std::isfinite(age)) age=0;
    const float t=std::clamp(age,0.0f,DebrisLifetime),angle=piece*2.39996323f;
    const glm::vec3 velocity(std::cos(angle)*(1.2f+.2f*piece),1.4f+.25f*(piece%3),std::sin(angle)*1.3f);
    const auto offset=velocity*t+glm::vec3(0,-2.2f*t*t,0);
    const float scale=std::clamp((DebrisLifetime-t)/.4f,0.0f,1.0f);
    return glm::scale(glm::rotate(glm::translate(world,offset),t*(2+piece),glm::normalize(glm::vec3(1,.4f+piece,.7f))),glm::vec3(scale));
}
}
