// Assimp로 모델을 읽고 정점·삼각형·재질 자료로 옮긴다. 게임 규칙은 처리하지 않는다.
#include "model_loader.h"
#include "model_binary.h"

#include <assimp/cimport.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <assimp/version.h>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <bcrypt.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <memory>
#include <string_view>
#include <utility>

namespace aegis
{
namespace
{

constexpr unsigned int ImportFlags = aiProcess_Triangulate | aiProcess_JoinIdenticalVertices |
                                     aiProcess_GenNormals | aiProcess_ValidateDataStructure;

std::string Utf8Path(const std::filesystem::path& path)
{
    const auto utf8Path = path.u8string();
    return {utf8Path.begin(), utf8Path.end()};
}

bool IsObj(const std::filesystem::path& path)
{
    auto extension = path.extension().wstring();
    for (auto& letter : extension)
    {
        if (letter >= L'A' && letter <= L'Z')
        {
            letter += L'a' - L'A';
        }
    }
    return extension == L".obj";
}

glm::vec3 ReadColor(const aiMaterial& material, const char* key, unsigned int type, unsigned int index,
                    const glm::vec3& fallback)
{
    aiColor3D color;
    if (material.Get(key, type, index, color) == AI_SUCCESS)
    {
        return {color.r, color.g, color.b};
    }
    return fallback;
}

float UnitValue(float value, float fallback)
{
    return std::isfinite(value) ? std::clamp(value, 0.0f, 1.0f) : fallback;
}

float SrgbToLinear(float value)
{
    return value <= 0.04045f ? value / 12.92f : std::pow((value + 0.055f) / 1.055f, 2.4f);
}

ModelMaterial ReadMaterial(const aiMaterial& source, bool obj)
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
    if (source.GetTexture(aiTextureType_DIFFUSE, 0, &texture) == AI_SUCCESS)
    {
        material.diffuseTexture = texture.C_Str();
    }

    // 현재 게임의 OBJ/MTL 색은 sRGB로 취급한다. glTF baseColorFactor는 이미 선형 RGB다.
    const auto base = ReadColor(source, AI_MATKEY_BASE_COLOR, material.diffuse);
    for (glm::length_t channel = 0; channel < base.length(); ++channel)
    {
        const float value = UnitValue(base[channel], UnitValue(material.diffuse[channel], 1.0f));
        material.baseColor[channel] = obj ? SrgbToLinear(value) : value;
    }
    float metallic{}, roughness{};
    if (source.Get(AI_MATKEY_METALLIC_FACTOR, metallic) == AI_SUCCESS)
    {
        material.metallic = UnitValue(metallic, material.metallic);
    }
    if (source.Get(AI_MATKEY_ROUGHNESS_FACTOR, roughness) == AI_SUCCESS)
    {
        material.roughness = UnitValue(roughness, material.roughness);
    }
    return material;
}

struct CloseFile
{
    void operator()(void* file) const
    {
        CloseHandle(file);
    }
};

using FileHandle = std::unique_ptr<void, CloseFile>;

FileHandle OpenSourceFile(const std::filesystem::path& path)
{
    // 해시를 계산하고 Assimp로 읽는 동안 다른 프로세스의 쓰기·삭제를 막는다.
    const HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                                    FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
    return FileHandle(file == INVALID_HANDLE_VALUE ? nullptr : file);
}

class Sha256
{
public:
    Sha256()
    {
        if (BCryptCreateHash(BCRYPT_SHA256_ALG_HANDLE, &hash, nullptr, 0, nullptr, 0, 0) != 0)
        {
            hash = nullptr;
        }
    }

    ~Sha256()
    {
        if (hash)
        {
            BCryptDestroyHash(hash);
        }
    }

    Sha256(const Sha256&) = delete;
    Sha256& operator=(const Sha256&) = delete;

    bool Add(const void* data, ULONG size)
    {
        auto* bytes = static_cast<const UCHAR*>(data);
        return hash && BCryptHashData(hash, const_cast<UCHAR*>(bytes), size, 0) == 0;
    }

    bool AddPath(const std::filesystem::path& path)
    {
        const auto name = path.u8string();
        return Add(name.c_str(), static_cast<ULONG>(name.size() + 1));
    }

    bool AddFile(HANDLE file)
    {
        LARGE_INTEGER size{};
        if (!GetFileSizeEx(file, &size) || !Add(&size.QuadPart, sizeof(size.QuadPart)))
        {
            return false;
        }

        std::array<UCHAR, 65536> buffer;
        DWORD read{};
        while (ReadFile(file, buffer.data(), static_cast<DWORD>(buffer.size()), &read, nullptr))
        {
            if (read == 0)
            {
                return true;
            }
            if (!Add(buffer.data(), read))
            {
                break;
            }
        }
        return false;
    }

    bool Finish(ModelFingerprint& fingerprint)
    {
        return hash &&
               BCryptFinishHash(hash, fingerprint.data(), static_cast<ULONG>(fingerprint.size()), 0) == 0;
    }

private:
    BCRYPT_HASH_HANDLE hash{};
};

std::string Hex(const ModelFingerprint& fingerprint)
{
    constexpr char digits[] = "0123456789abcdef";
    std::string text;
    text.reserve(fingerprint.size() * 2);
    for (const auto byte : fingerprint)
    {
        text.push_back(digits[byte >> 4]);
        text.push_back(digits[byte & 15]);
    }
    return text;
}

std::string_view Trim(std::string_view text)
{
    const auto first = text.find_first_not_of(" \t");
    return first == std::string_view::npos ? std::string_view{}
                                           : text.substr(first, text.find_last_not_of(" \t") - first + 1);
}

bool CheckMaterialField(std::string_view field, std::string_view value, std::vector<std::string>& materials)
{
    if (value.empty())
        return false;
    if (field == "newmtl")
    {
        materials.emplace_back(value);
        return true;
    }
    if (materials.empty() || value.starts_with("spectral"))
        return false;
    const std::string_view allowed[]{"Ka", "Kd", "Ks", "Ke", "Tf", "Ns", "Ni", "d",
                                     "Tr", "illum", "Pm", "Pr", "Ps", "Pc", "Pcr"};
    for (auto name : allowed)
        if (name == field)
            return true;
    return false;
}

bool CheckObjectField(std::string_view field, std::string_view value, const std::string& mtlName,
                      const std::vector<std::string>& materials, bool& foundMtl)
{
    if (field == "mtllib")
    {
        if (foundMtl || value != mtlName)
            return false;
        foundMtl = true;
        return true;
    }
    if (field == "usemtl")
    {
        if (!foundMtl)
            return false;
        for (const auto& name : materials)
            if (name == value)
                return true;
        return false;
    }
    const std::string_view allowed[]{"v", "vt", "vn", "f", "l", "p", "o", "g", "s"};
    for (auto name : allowed)
        if (name == field)
            return true;
    return false;
}

bool CheckSourceDirectives(const std::filesystem::path& path, bool readingMaterials,
                           const std::string& mtlName, std::vector<std::string>& materials)
{
    bool foundMtl = false;
    std::ifstream file(path, std::ios::binary);
    std::string line;
    bool firstLine = true;
    while (std::getline(file, line))
    {
        std::string_view text(line);
        if (firstLine && text.starts_with("\xEF\xBB\xBF"))
        {
            text.remove_prefix(3);
        }
        firstLine = false;
        if (text.ends_with('\r'))
        {
            text.remove_suffix(1);
        }
        text = Trim(text);
        if (text.find_first_of(std::string_view("\0\r\v\f\x1A", 5)) != std::string_view::npos ||
            text.ends_with('\\'))
        {
            return false;
        }
        if (text.empty() || text.starts_with('#'))
        {
            continue;
        }
        const auto field = text.substr(0, text.find_first_of(" \t"));
        const auto value = Trim(text.substr(field.size()));
        if (readingMaterials)
        {
            if (!CheckMaterialField(field, value, materials))
                return false;
        }
        else if (!CheckObjectField(field, value, mtlName, materials, foundMtl))
            return false;
    }
    if (!file.eof() || file.bad())
        return false;
    return readingMaterials ? !materials.empty() : foundMtl;
}

bool HasOnlyExpectedDependencies(const std::filesystem::path& objPath, const std::filesystem::path& mtlPath)
{
    // 이 캐시는 단일 MTL과 외부 텍스처가 없는 OBJ만 지원한다.
    const auto mtlName = Utf8Path(mtlPath.filename());
    std::vector<std::string> materials;
    return CheckSourceDirectives(mtlPath, true, mtlName, materials) &&
           CheckSourceDirectives(objPath, false, mtlName, materials);
}

} // namespace

bool LoadModel(const std::filesystem::path& path, ModelData& model, std::string& error)
{
    const std::string fileName = Utf8Path(path);
    // Assimp가 할당한 장면은 함수가 끝날 때 aiReleaseImport로 반환한다.
    const std::unique_ptr<const aiScene, decltype(&aiReleaseImport)> scene(
        aiImportFile(fileName.c_str(), ImportFlags), aiReleaseImport);

    if (!scene)
    {
        error = fileName + ": " + aiGetErrorString();
        return false;
    }

    ModelData loaded;
    const bool obj = IsObj(path);
    loaded.materials.reserve(scene->mNumMaterials);
    for (unsigned int i = 0; i < scene->mNumMaterials; ++i)
    {
        loaded.materials.push_back(ReadMaterial(*scene->mMaterials[i], obj));
    }

    loaded.meshes.reserve(scene->mNumMeshes);
    for (unsigned int i = 0; i < scene->mNumMeshes; ++i)
    {
        const aiMesh& source = *scene->mMeshes[i];
        ModelMesh mesh;
        mesh.name = source.mName.C_Str();
        mesh.materialIndex = source.mMaterialIndex;
        mesh.hasUvs = source.HasTextureCoords(0);
        mesh.indices.reserve(static_cast<std::size_t>(source.mNumFaces) * 3);

        for (unsigned int faceIndex = 0; faceIndex < source.mNumFaces; ++faceIndex)
        {
            const aiFace& face = source.mFaces[faceIndex];
            if (face.mNumIndices == 3)
            {
                mesh.indices.insert(mesh.indices.end(), face.mIndices, face.mIndices + 3);
            }
        }
        if (mesh.indices.empty())
        {
            continue;
        }

        const bool hasNormals = source.HasNormals();
        mesh.vertices.reserve(source.mNumVertices);
        for (unsigned int vertexIndex = 0; vertexIndex < source.mNumVertices; ++vertexIndex)
        {
            ModelVertex vertex;
            const auto& position = source.mVertices[vertexIndex];
            vertex.position = {position.x, position.y, position.z};
            if (hasNormals)
            {
                const auto& normal = source.mNormals[vertexIndex];
                vertex.normal = {normal.x, normal.y, normal.z};
            }
            if (mesh.hasUvs)
            {
                const auto& uv = source.mTextureCoords[0][vertexIndex];
                vertex.uv = {uv.x, uv.y};
            }
            mesh.vertices.push_back(vertex);
        }
        loaded.meshes.push_back(std::move(mesh));
    }

    if (loaded.meshes.empty())
    {
        error = fileName + ": no triangle meshes";
        return false;
    }

    model = std::move(loaded);
    error.clear();
    return true;
}

ObjModelCache::ObjModelCache(const std::filesystem::path& cacheDirectory,
                             const std::filesystem::path& materialPath)
    : cacheDirectory(cacheDirectory), materialPath(materialPath)
{
}

bool ObjModelCache::Load(const std::filesystem::path& path, ModelData& model, std::string& error,
                         bool& cacheHit) const
{
    cacheHit = false;
    if (cacheDirectory.empty() || !IsObj(path))
    {
        return LoadModel(path, model, error);
    }

    std::error_code status;
    const auto objPath = std::filesystem::canonical(path, status);
    if (status || !std::filesystem::is_regular_file(objPath, status))
    {
        return LoadModel(path, model, error);
    }
    const auto mtlPath = std::filesystem::canonical(materialPath, status);
    if (status || !std::filesystem::is_regular_file(mtlPath, status) ||
        mtlPath != objPath.parent_path() / materialPath.filename())
    {
        return LoadModel(path, model, error);
    }

    // 원본 호출 경로의 형상 이름과 상대 MTL 해석이 같은 경우에만 캐시를 공유한다.
    const auto originalDirectory = std::filesystem::canonical(
        path.has_parent_path() ? path.parent_path() : std::filesystem::path(L"."), status);
    if (status || originalDirectory != objPath.parent_path() || path.filename() != objPath.filename())
    {
        return LoadModel(path, model, error);
    }

    const auto objFile = OpenSourceFile(objPath);
    const auto mtlFile = OpenSourceFile(mtlPath);
    if (!objFile || !mtlFile)
    {
        return LoadModel(path, model, error);
    }

    Sha256 identityHash;
    ModelFingerprint identity{};
    if (!identityHash.AddPath(objPath) || !identityHash.AddPath(mtlPath) || !identityHash.Finish(identity))
    {
        return LoadModel(path, model, error);
    }

    // 파싱 규칙 또는 색 변환이 바뀌면 이 버전을 올린다.
    constexpr char ImportPolicy[] = "AEGIS OBJ import v1";
    const unsigned int importer[]{aiGetVersionMajor(),    aiGetVersionMinor(), aiGetVersionPatch(),
                                  aiGetVersionRevision(), aiGetCompileFlags(), ImportFlags};
    Sha256 sourceHash;
    ModelFingerprint fingerprint{};
    if (!sourceHash.Add(ImportPolicy, sizeof(ImportPolicy)) ||
        !sourceHash.Add(identity.data(), static_cast<ULONG>(identity.size())) ||
        !sourceHash.Add(importer, sizeof(importer)) || !sourceHash.AddFile(objFile.get()) ||
        !sourceHash.AddFile(mtlFile.get()) || !sourceHash.Finish(fingerprint))
    {
        return LoadModel(path, model, error);
    }

    const auto cachePath = cacheDirectory / (Hex(identity) + ".bin");
    if (ReadModelBinary(cachePath, fingerprint, model))
    {
        cacheHit = true;
        error.clear();
        return true;
    }
    if (!HasOnlyExpectedDependencies(objPath, mtlPath))
    {
        return LoadModel(path, model, error);
    }
    if (!LoadModel(objPath, model, error))
    {
        return false;
    }

    std::filesystem::create_directories(cacheDirectory, status);
    if (!status)
    {
        WriteModelBinary(cachePath, fingerprint, model);
    }
    return true;
}

} // namespace aegis
