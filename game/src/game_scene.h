#pragma once

#include "drone.h"
#include "player_camera.h"

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
    Drone drone;
    PlayerCamera camera;
};

} // namespace aegis
