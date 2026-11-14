#pragma once

#include "drone.h"
#include "player_camera.h"

#include <array>
#include <filesystem>

namespace aegis {

class GameScene {
public:
    bool Init(const std::filesystem::path& projectDirectory);
    void Update(double deltaTime);
    void Draw(int width, int height) const;
    void Reset();
    void Clear();

private:
    struct CorridorObject {
        glm::mat4 model{1.0f};
        std::vector<DrawMesh> meshes;
    };

    bool InitCorridor(const std::filesystem::path& projectDirectory);

    MeshRenderer renderer;
    std::array<CorridorObject, 5> corridor;
    Drone drone;
    PlayerCamera camera;
};

} // namespace aegis
