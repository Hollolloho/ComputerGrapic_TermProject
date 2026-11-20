#include "framework/FrameWork.h"
#include "loader/model_loader.h"

#include <iostream>
#include <string_view>

namespace
{

int InspectModel(const std::filesystem::path& path)
{
    aegis::ModelData model;
    std::string error;
    if (!aegis::LoadModel(path, model, error))
    {
        std::cerr << error << '\n';
        return 1;
    }

    std::size_t vertices{};
    std::size_t triangles{};
    for (const auto& mesh : model.meshes)
    {
        vertices += mesh.vertices.size();
        triangles += mesh.indices.size() / 3;
    }
    std::cout << "meshes=" << model.meshes.size() << " vertices=" << vertices << " triangles=" << triangles
              << " materials=" << model.materials.size() << '\n';

    for (const auto& material : model.materials)
    {
        std::cout << material.name << " Kd=" << material.diffuse.r << ',' << material.diffuse.g << ','
                  << material.diffuse.b << " opacity=" << material.opacity;
        if (!material.diffuseTexture.empty())
        {
            std::cout << " map_Kd=" << material.diffuseTexture;
        }
        std::cout << '\n';
    }
    return 0;
}

} // namespace

int wmain(int argc, wchar_t* argv[])
{
    if (argc == 1)
    {
        GameFrameWork game;
        return game.Run();
    }
    const std::wstring_view mode = argv[1];
    GameRunOptions options;
    options.campaign = false;
    options.droneTest = false;
    bool knownMode = true;
    if (mode == L"--campaign-test")
    {
        options.campaign = true;
        options.droneTest = true;
    }
    else if (mode == L"--combat-map-test")
    {
        options.combatMapTest = true;
        options.droneTest = true;
    }
    else if (mode == L"--player-view-test")
    {
        options.playerViewTest = true;
    }
    else if (mode == L"--boss-test")
    {
        options.bossTest = true;
    }
    else if (mode == L"--drone-test")
    {
        options.droneTest = true;
    }
    else if (mode == L"--corridor-test")
    { /* 기존 복도와 고정 포탑을 표시한다. */
    }
    else if (mode == L"--combat-test")
    {
        options.combatTest = true;
    }
    else if (mode == L"--turret-test")
    {
        options.combatTest = true;
        options.turretTest = true;
    }
    else
    {
        knownMode = false;
    }

    if (knownMode)
    {
        if (argc > 3 || (argc == 3 && std::wstring_view(argv[2]) != L"--smoke-test"))
        {
            std::cerr << "Usage: AEGIS.exe <test mode> [--smoke-test]\n";
            return 2;
        }
        options.smokeTest = (argc == 3);
        GameFrameWork game;
        return game.Run(options);
    }
    if (argc == 3 && std::wstring_view(argv[1]) == L"--inspect-obj")
    {
        return InspectModel(argv[2]);
    }
    std::cerr << "Usage: AEGIS.exe [--inspect-obj <model.obj> | "
                 "--player-view-test|--combat-map-test|--campaign-test|--boss-test|--drone-test|--corridor-"
                 "test|--combat-test|--turret-test [--smoke-test]]\n";
    return 2;
}
