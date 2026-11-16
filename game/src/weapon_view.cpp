#include "weapon_view.h"
#include "cw01c_layout.generated.h"
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>
namespace aegis {
bool WeaponView::Init(const std::filesystem::path& root) {
    std::array<AtlasPart,weapon_asset::parts.size()> parts;
    std::array<AtlasTexture,5> textures;
    for(std::size_t i=0;i<parts.size();++i) parts[i]={weapon_asset::parts[i].mesh,weapon_asset::parts[i].triangles};
    for(std::size_t i=0;i<textures.size();++i) {
        const auto& t=weapon_asset::textures[i]; textures[i]={t.name,t.file,t.srgb};
    }
    Reset(); return atlas.Init(root,root/"asset/cw01c/runtime",parts,textures,weapon_asset::emissionMultiplier);
}
void WeaponView::Reset() { recoil=0; shotCount=0; }
void WeaponView::OnShot() { recoil=1; ++shotCount; }

void WeaponView::Update(double realDt)
{
    if (std::isfinite(realDt) && realDt>=0) recoil*=std::exp(-14.0f*static_cast<float>(realDt));
}

void WeaponView::Draw(float aspect) const
{
    if (aspect<=0) return;
    const glm::mat4 projection=glm::perspective(glm::radians(60.0f),aspect,0.02f,3.0f);
    glm::mat4 mount=glm::translate(glm::mat4(1),glm::vec3(0.16f,-0.16f,-0.50f)
        +glm::vec3(0,-0.008f*recoil,0.035f*recoil));
    mount=glm::rotate(mount,glm::pi<float>()+0.25f,glm::vec3(0,1,0));
    mount=glm::rotate(mount,glm::radians(-4.0f+recoil*5.0f),glm::vec3(1,0,0));
    // Viewmodel has its own depth pass; it must not inherit distant scene depth.
    glClear(GL_DEPTH_BUFFER_BIT); glEnable(GL_DEPTH_TEST); glDepthMask(GL_TRUE);
    DrawMeshes(projection,mount,recoil);
}

void WeaponView::DrawWorld(const glm::mat4& projection,const glm::mat4& view,const glm::vec3& position,float yawDegrees) const
{
    const auto model=glm::rotate(glm::translate(glm::mat4(1),position),glm::radians(yawDegrees),glm::vec3(0,1,0));
    DrawMeshes(projection,view*model,0);
}

void WeaponView::DrawMeshes(const glm::mat4& projection,const glm::mat4& mount,float glow) const {
    std::array<glm::mat4,weapon_asset::parts.size()> matrices;
    for(std::size_t i=0;i<matrices.size();++i) matrices[i]=glm::translate(mount,weapon_asset::parts[i].offset);
    atlas.Draw(projection,glm::mat4(1),matrices,glow);
}
void WeaponView::Clear() { atlas.Clear(); Reset(); }
} // namespace aegis
