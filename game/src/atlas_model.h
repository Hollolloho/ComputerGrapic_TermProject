#pragma once
#include <GL/glew.h>
#include <glm/mat4x4.hpp>
#include <array>
#include <filesystem>
#include <span>
#include <vector>
namespace aegis {
struct AtlasPart { const char* mesh; int triangles; };
struct AtlasTexture { const char* name; const char* file; bool srgb; };
// Shared five-map PBR resources; gameplay and rig transforms remain external.
class AtlasModel {
public:
    AtlasModel()=default;
    AtlasModel(const AtlasModel&)=delete;
    AtlasModel& operator=(const AtlasModel&)=delete;
    bool Init(const std::filesystem::path& root,const std::filesystem::path& runtime,
        std::span<const AtlasPart> parts,std::span<const AtlasTexture> textureSpecs,float emission);
    void Draw(const glm::mat4& projection,const glm::mat4& view,
        std::span<const glm::mat4> partMatrices,float glow=0,std::span<const float> emissionScales={}) const;
    void Clear();
private:
    struct Mesh { GLuint vao{},vbo{},ebo{}; GLsizei indices{}; std::size_t partIndex{}; };
    std::vector<Mesh> meshes;
    std::array<GLuint,5> textures{};
    GLuint program{};
    GLint transformLocation{-1},modelLocation{-1},shotLocation{-1},emissionLocation{-1};
    float baseEmission{1};
};
}
