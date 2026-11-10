#include "model_loader.h"

#include <assimp/cimport.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include <memory>
#include <utility>

namespace aegis {
namespace {

glm::vec3 ReadColor(const aiMaterial& material, const char* key, unsigned int type,
                    unsigned int index, const glm::vec3& fallback)
{
    aiColor3D color;
    if (material.Get(key, type, index, color) == AI_SUCCESS) {
        return {color.r, color.g, color.b};
    }
    return fallback;
}

ModelMaterial ReadMaterial(const aiMaterial& source)
{
    ModelMaterial material;
    material.name = source.GetName().C_Str();
    material.ambient = ReadColor(source, AI_MATKEY_COLOR_AMBIENT, material.ambient);
    material.diffuse = ReadColor(source, AI_MATKEY_COLOR_DIFFUSE, material.diffuse);
    material.specular = ReadColor(source, AI_MATKEY_COLOR_SPECULAR, material.specular);
    material.emissive = ReadColor(source, AI_MATKEY_COLOR_EMISSIVE, material.emissive);
    source.Get(AI_MATKEY_SHININESS, material.shininess);
    source.Get(AI_MATKEY_OPACITY, material.opacity);

    aiString texture;
    if (source.GetTexture(aiTextureType_DIFFUSE, 0, &texture) == AI_SUCCESS) {
        material.diffuseTexture = texture.C_Str();
    }
    return material;
}

} // namespace

bool LoadModel(const std::filesystem::path& path, ModelData& model, std::string& error)
{
    const auto utf8Path = path.u8string();
    const std::string fileName(utf8Path.begin(), utf8Path.end());
    constexpr unsigned int importFlags = aiProcess_Triangulate |
        aiProcess_JoinIdenticalVertices | aiProcess_GenNormals | aiProcess_ValidateDataStructure;
    const std::unique_ptr<const aiScene, decltype(&aiReleaseImport)> scene(
        aiImportFile(fileName.c_str(), importFlags), aiReleaseImport);

    if (!scene) {
        error = fileName + ": " + aiGetErrorString();
        return false;
    }

    ModelData loaded;
    loaded.materials.reserve(scene->mNumMaterials);
    for (unsigned int i = 0; i < scene->mNumMaterials; ++i) {
        loaded.materials.push_back(ReadMaterial(*scene->mMaterials[i]));
    }

    loaded.meshes.reserve(scene->mNumMeshes);
    for (unsigned int i = 0; i < scene->mNumMeshes; ++i) {
        const aiMesh& source = *scene->mMeshes[i];
        ModelMesh mesh;
        mesh.name = source.mName.C_Str();
        mesh.materialIndex = source.mMaterialIndex;
        mesh.hasUvs = source.HasTextureCoords(0);
        mesh.indices.reserve(static_cast<std::size_t>(source.mNumFaces) * 3);

        for (unsigned int faceIndex = 0; faceIndex < source.mNumFaces; ++faceIndex) {
            const aiFace& face = source.mFaces[faceIndex];
            if (face.mNumIndices == 3) {
                mesh.indices.insert(mesh.indices.end(), face.mIndices, face.mIndices + 3);
            }
        }
        if (mesh.indices.empty()) {
            continue;
        }

        const bool hasNormals = source.HasNormals();
        mesh.vertices.reserve(source.mNumVertices);
        for (unsigned int vertexIndex = 0; vertexIndex < source.mNumVertices; ++vertexIndex) {
            ModelVertex vertex;
            const auto& position = source.mVertices[vertexIndex];
            vertex.position = {position.x, position.y, position.z};
            if (hasNormals) {
                const auto& normal = source.mNormals[vertexIndex];
                vertex.normal = {normal.x, normal.y, normal.z};
            }
            if (mesh.hasUvs) {
                const auto& uv = source.mTextureCoords[0][vertexIndex];
                vertex.uv = {uv.x, uv.y};
            }
            mesh.vertices.push_back(vertex);
        }
        loaded.meshes.push_back(std::move(mesh));
    }

    if (loaded.meshes.empty()) {
        error = fileName + ": no triangle meshes";
        return false;
    }

    model = std::move(loaded);
    error.clear();
    return true;
}

} // namespace aegis
