#pragma once

#include "combat_map/CombatMap.h"
#include "gameplay/player/player_camera.h"
#include "gameplay/rendering/mesh_renderer.h"
#include <array>

namespace aegis
{
// 입구, 복도와 전투 맵의 표시 자원을 관리한다.
class CampaignEnvironment
{
public:
    bool Init(const std::filesystem::path& root);
    void SetEntryCamera();
    void DrawEntry(int width, int height) const;
    void DrawBattle(const glm::mat4& viewProjection, const glm::vec3& eye,
                    bool useCombatMap, float doorProgress) const;
    void Clear();

private:
    struct CorridorObject
    {
        glm::mat4 model{1.0f};
        std::vector<DrawMesh> meshes;
    };

    bool InitCorridor(const std::filesystem::path& root);
    bool InitEntry(const std::filesystem::path& root);

    std::vector<DrawMesh> entryRoom, entryTerminal;
    std::array<CorridorObject, 5> corridor;
    CombatMap combatMap;
    MeshRenderer renderer;
    PlayerCamera camera;
};
} // namespace aegis
