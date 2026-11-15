#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

namespace aegis {

struct ModelVertex {
    glm::vec3 position{0.0f};
    glm::vec3 normal{0.0f};
    glm::vec2 uv{0.0f};
};

struct ModelMesh {
    std::string name;
    std::vector<ModelVertex> vertices;
    std::vector<unsigned int> indices;
    unsigned int materialIndex{};
    bool hasUvs{};
};

struct ModelMaterial {
    std::string name;
    glm::vec3 ambient{0.0f};
    glm::vec3 diffuse{1.0f};
    glm::vec3 specular{0.0f};
    glm::vec3 emissive{0.0f};
    float shininess{};
    float opacity{1.0f};
    std::string diffuseTexture;
    glm::vec3 baseColor{1.0f}; // linear RGB
    float metallic{};
    float roughness{0.5f};
};

struct ModelData {
    std::vector<ModelMesh> meshes;
    std::vector<ModelMaterial> materials;
};

// 결과는 Assimp 장면과 독립적이다. 읽기에 실패하면 기존 model을 유지한다.
bool LoadModel(const std::filesystem::path& path, ModelData& model, std::string& error);

// 단일 인접 MTL과 외부 텍스처가 없는 OBJ의 로컬 캐시다.
// 원본 지문이 같으면 바이너리를 읽고, 캐시를 사용할 수 없으면 LoadModel로 돌아간다.
class ObjModelCache {
public:
    ObjModelCache(const std::filesystem::path& cacheDirectory,
                  const std::filesystem::path& materialPath);
    bool Load(const std::filesystem::path& path, ModelData& model, std::string& error,
              bool& cacheHit) const;

private:
    std::filesystem::path cacheDirectory;
    std::filesystem::path materialPath;
};

} // namespace aegis
