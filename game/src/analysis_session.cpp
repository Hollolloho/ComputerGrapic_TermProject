#include "analysis_session.h"
#include <cmath>
#include <glm/geometric.hpp>

namespace aegis::combat {
std::optional<float> AnalysisSession::TargetDistance(const Collider& terminal,const glm::vec3& eye,
    const glm::vec3& direction,std::span<const Collider> obstacles) const {
    const float length2=glm::dot(direction,direction);
    if (!std::isfinite(length2) || length2<=1e-8f) return {};
    const auto end=eye+glm::normalize(direction)*2.5f;
    const auto target=FindFirstHit(eye,end,std::span<const Collider>(&terminal,1));
    if (!target) return {};
    const auto blocked=FindFirstHit(eye,end,obstacles);
    if (blocked && blocked->objectId!=terminal.id && blocked->fraction<=target->fraction) return {};
    return target->fraction*2.5f;
}
bool AnalysisSession::IsFirstTarget(const Collider& terminal,const glm::vec3& eye,const glm::vec3& direction,
    std::span<const Collider> obstacles,std::optional<float> weaponDistance) const {
    const auto target=TargetDistance(terminal,eye,direction,obstacles);
    return target && (!weaponDistance || *target<=*weaponDistance);
}
bool AnalysisSession::Open(bool safe,const Collider& terminal,const glm::vec3& eye,const glm::vec3& direction,
    std::span<const Collider> obstacles,std::optional<float> weaponDistance) {
    if (active || !safe || !IsFirstTarget(terminal,eye,direction,obstacles,weaponDistance)) return false;
    active=true; ++entries; return true;
}
bool AnalysisSession::Close() {
    if (!active) return false;
    active=false; ++exits; return true;
}
}
