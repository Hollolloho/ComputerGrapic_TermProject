#include "game_framework.h"
#include "model_loader.h"

#include <iostream>
#include <string_view>

namespace {

int InspectModel(const std::filesystem::path& path)
{
    aegis::ModelData model;
    std::string error;
    if (!aegis::LoadModel(path, model, error)) {
        std::cerr << error << '\n';
        return 1;
    }

    std::size_t vertices{};
    std::size_t triangles{};
    for (const auto& mesh : model.meshes) {
        vertices += mesh.vertices.size();
        triangles += mesh.indices.size() / 3;
    }
    std::cout << "meshes=" << model.meshes.size() << " vertices=" << vertices
              << " triangles=" << triangles << " materials=" << model.materials.size() << '\n';

    for (const auto& material : model.materials) {
        std::cout << material.name << " Kd=" << material.diffuse.r << ',' << material.diffuse.g
                  << ',' << material.diffuse.b << " opacity=" << material.opacity;
        if (!material.diffuseTexture.empty()) {
            std::cout << " map_Kd=" << material.diffuseTexture;
        }
        std::cout << '\n';
    }
    return 0;
}

} // namespace

int wmain(int argc, wchar_t* argv[])
{
    if (argc == 1) {
        aegis::GameFramework game;
        return game.Run();
    }
    if (argc >= 2 && (std::wstring_view(argv[1]) == L"--combat-map-test" || std::wstring_view(argv[1]) == L"--campaign-test" || std::wstring_view(argv[1]) == L"--boss-test" || std::wstring_view(argv[1]) == L"--drone-test" || std::wstring_view(argv[1]) == L"--corridor-test" || std::wstring_view(argv[1]) == L"--combat-test" || std::wstring_view(argv[1]) == L"--turret-test")) {
        if (argc > 3 || (argc == 3 && std::wstring_view(argv[2]) != L"--smoke-test")) {
            std::cerr << "Usage: AEGIS.exe --combat-map-test|--campaign-test|--boss-test|--drone-test|--corridor-test|--combat-test|--turret-test [--smoke-test]\n";
            return 2;
        }
        aegis::GameFramework game;
        const auto mode=std::wstring_view(argv[1]);
        return game.Run(mode==L"--combat-test" || mode==L"--turret-test",argc==3,mode==L"--turret-test",
            mode==L"--drone-test" || mode==L"--campaign-test" || mode==L"--combat-map-test",mode==L"--boss-test",mode==L"--campaign-test",mode==L"--combat-map-test");
    }
    if (argc == 3 && std::wstring_view(argv[1]) == L"--inspect-obj") {
        return InspectModel(argv[2]);
    }
    std::cerr << "Usage: AEGIS.exe [--inspect-obj <model.obj> | --combat-map-test|--campaign-test|--boss-test|--drone-test|--corridor-test|--combat-test|--turret-test [--smoke-test]]\n";
    return 2;
}
