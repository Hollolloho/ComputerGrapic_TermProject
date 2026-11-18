#include "world_signs.h"
namespace aegis {
bool WorldSigns::Init() { Clear();for(auto& sign:signs) if(!sign.Init()) { Clear();return false; }return true; }
void WorldSigns::Clear() { for(auto& sign:signs) sign.Clear(); }
void WorldSigns::Draw(const glm::mat4& vp,const glm::vec3& supply,const glm::vec3& terminal,bool safe) const {
    signs[0].DrawWorld(L"무기 보급",L"빈손으로 조준 + E",vp,supply+glm::vec3(0,.68f,-.02f),{.45f,.18f});
    signs[1].DrawWorld(L"드론 분석",safe ? L"가까이서 조준 + E":L"적과 적탄 정리 후 사용",vp,terminal+glm::vec3(0,1.95f,-.02f),{.45f,.18f});
    signs[2].DrawWorld(L"코어실 진행",safe ? L"Enter: 코어실로 이동":L"안전 확보 후 Enter",vp,{0,2.55f,-17.5f},{2.8f,.8f});
}
}
