#pragma once
#include "korean_ui.h"
#include <array>
namespace aegis {
class WorldSigns {
public:
    bool Init();
    void Draw(const glm::mat4& vp,const glm::vec3& supply,const glm::vec3& terminal,bool safe) const;
    void Clear();
private:
    mutable std::array<KoreanUi,3> signs;
};
}
