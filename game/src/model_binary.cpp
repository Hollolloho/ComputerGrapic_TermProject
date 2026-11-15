#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include "model_binary.h"
#include "model_loader.h"

#include <algorithm>
#include <atomic>
#include <bit>
#include <cmath>
#include <limits>
#include <memory>
#include <new>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace aegis {
namespace {

// 헤더 순서는 매직, 버전, 원본 지문, 본문 크기, 본문 체크섬이다.
// 수치 필드는 리틀 엔디언, 문자열은 바이트 길이가 붙은 UTF-8로 저장한다.
constexpr std::string_view Magic = "AEGMODEL";
constexpr std::uint32_t Version = 1;
constexpr std::size_t HeaderBytes = 56;
constexpr std::size_t MaxPayloadBytes = 128 * 1024 * 1024;
constexpr std::size_t MaxStringBytes = 64 * 1024;
constexpr std::uint32_t MaxMaterials = 65'536;
constexpr std::uint32_t MaxMeshes = 100'000;
constexpr std::size_t CountBytes = 4;
constexpr std::size_t MaterialBytes = 2 * CountBytes + (5 * 3 + 4) * 4;
constexpr std::size_t MeshHeaderBytes = 4 * CountBytes + 1;
constexpr std::size_t VertexBytes = (3 + 3 + 2) * 4;
constexpr std::size_t IndexBytes = 4;
constexpr std::size_t MinMeshBytes = MeshHeaderBytes + VertexBytes + 3 * IndexBytes;

static_assert(sizeof(float) == 4 && std::numeric_limits<float>::is_iec559);
static_assert(sizeof(unsigned int) == sizeof(std::uint32_t));
static_assert(HeaderBytes == Magic.size() + 2 * CountBytes + ModelFingerprint{}.size() + 8);

using Bytes = std::vector<std::uint8_t>;
using ByteView = std::span<const std::uint8_t>;

bool IsUtf8(std::string_view value)
{
    return value.size() <= MaxStringBytes &&
           value.find('\0') == std::string_view::npos &&
           (value.empty() || MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
                                                 static_cast<int>(value.size()), nullptr, 0) > 0);
}

bool IsUnit(float value)
{
    return value >= 0.0f && value <= 1.0f;
}

bool ValidMaterialRanges(const ModelMaterial& material)
{
    return IsUnit(material.baseColor.r) && IsUnit(material.baseColor.g) &&
           IsUnit(material.baseColor.b) && IsUnit(material.opacity) &&
           IsUnit(material.metallic) && IsUnit(material.roughness) &&
           material.shininess >= 0.0f;
}

std::uint64_t PayloadChecksum(ByteView bytes)
{
    // FNV-1a로 형식이 유효해 보이는 비트 손상도 검출한다.
    std::uint64_t checksum = 14695981039346656037ull;
    for (const std::uint8_t byte : bytes) {
        checksum ^= byte;
        checksum *= 1099511628211ull;
    }
    return checksum;
}

class BinaryReader {
public:
    explicit BinaryReader(ByteView bytes) : bytes(bytes) {}

    std::size_t Remaining() const { return bytes.size(); }

    ByteView Take(std::size_t size)
    {
        if (size > Remaining()) {
            return {};
        }
        const auto result = bytes.first(size);
        bytes = bytes.subspan(size);
        return result;
    }

    template<class T>
    bool ReadUnsigned(T& value)
    {
        const auto field = Take(sizeof(T));
        if (field.empty()) {
            return false;
        }
        std::uint64_t bits{};
        for (std::size_t i = 0; i < field.size(); ++i) {
            bits |= static_cast<std::uint64_t>(field[i]) << (8 * i);
        }
        value = static_cast<T>(bits);
        return true;
    }

    bool ReadFloat(float& value)
    {
        std::uint32_t bits{};
        if (!ReadUnsigned(bits)) {
            return false;
        }
        value = std::bit_cast<float>(bits);
        return std::isfinite(value);
    }

    bool ReadVec3(glm::vec3& value)
    {
        return ReadFloat(value.x) && ReadFloat(value.y) && ReadFloat(value.z);
    }

    bool ReadString(std::string& value)
    {
        std::uint32_t size{};
        if (!ReadUnsigned(size) || size > MaxStringBytes || size > Remaining()) {
            return false;
        }
        if (size == 0) {
            value.clear();
            return true;
        }
        const auto field = Take(size);
        const std::string_view text(reinterpret_cast<const char*>(field.data()), field.size());
        if (!IsUtf8(text)) {
            return false;
        }
        value.assign(text);
        return true;
    }

private:
    ByteView bytes;
};

class BinaryWriter {
public:
    explicit BinaryWriter(Bytes& bytes) : bytes(bytes) {}

    template<class T>
    void WriteUnsigned(T value)
    {
        auto bits = static_cast<std::uint64_t>(value);
        for (std::size_t i = 0; i < sizeof(T); ++i) {
            bytes.push_back(static_cast<std::uint8_t>(bits & 0xff));
            bits >>= 8;
        }
    }

    void WriteFloat(float value)
    {
        valid = valid && std::isfinite(value);
        WriteUnsigned(std::bit_cast<std::uint32_t>(value));
    }

    void WriteVec3(const glm::vec3& value)
    {
        WriteFloat(value.x);
        WriteFloat(value.y);
        WriteFloat(value.z);
    }

    void WriteString(std::string_view value)
    {
        WriteUnsigned(static_cast<std::uint32_t>(value.size()));
        bytes.insert(bytes.end(), value.begin(), value.end());
    }

    bool Valid() const { return valid; }

private:
    Bytes& bytes;
    bool valid = true;
};

bool ReadMaterial(BinaryReader& reader, ModelMaterial& material)
{
    return reader.ReadString(material.name) && reader.ReadString(material.diffuseTexture) &&
           reader.ReadVec3(material.ambient) && reader.ReadVec3(material.diffuse) &&
           reader.ReadVec3(material.specular) && reader.ReadVec3(material.emissive) &&
           reader.ReadVec3(material.baseColor) && reader.ReadFloat(material.shininess) &&
           reader.ReadFloat(material.opacity) && reader.ReadFloat(material.metallic) &&
           reader.ReadFloat(material.roughness) && ValidMaterialRanges(material);
}

bool ReadMesh(BinaryReader& reader, std::uint32_t materialCount, ModelMesh& mesh)
{
    std::uint8_t hasUvs{};
    std::uint32_t vertexCount{};
    std::uint32_t indexCount{};
    if (!reader.ReadString(mesh.name) || !reader.ReadUnsigned(mesh.materialIndex) ||
        mesh.materialIndex >= materialCount || !reader.ReadUnsigned(hasUvs) || hasUvs > 1 ||
        !reader.ReadUnsigned(vertexCount) || !reader.ReadUnsigned(indexCount) ||
        vertexCount == 0 || indexCount == 0 || indexCount % 3 != 0 ||
        vertexCount > reader.Remaining() / VertexBytes ||
        indexCount > (reader.Remaining() - vertexCount * VertexBytes) / IndexBytes) {
        return false;
    }

    // 남은 바이트로 수량을 확인한 뒤 메모리를 할당한다.
    mesh.hasUvs = hasUvs != 0;
    mesh.vertices.resize(vertexCount);
    for (auto& vertex : mesh.vertices) {
        if (!reader.ReadVec3(vertex.position) || !reader.ReadVec3(vertex.normal) ||
            !reader.ReadFloat(vertex.uv.x) || !reader.ReadFloat(vertex.uv.y)) {
            return false;
        }
    }
    mesh.indices.resize(indexCount);
    for (auto& index : mesh.indices) {
        if (!reader.ReadUnsigned(index) || index >= vertexCount) {
            return false;
        }
    }
    return true;
}

bool ReadPayload(ByteView bytes, ModelData& model)
{
    BinaryReader reader(bytes);
    std::uint32_t materialCount{};
    if (!reader.ReadUnsigned(materialCount) || materialCount == 0 ||
        materialCount > MaxMaterials || materialCount > reader.Remaining() / MaterialBytes) {
        return false;
    }
    model.materials.resize(materialCount);
    for (auto& material : model.materials) {
        if (!ReadMaterial(reader, material)) {
            return false;
        }
    }

    std::uint32_t meshCount{};
    if (!reader.ReadUnsigned(meshCount) || meshCount == 0 ||
        meshCount > MaxMeshes || meshCount > reader.Remaining() / MinMeshBytes) {
        return false;
    }
    model.meshes.resize(meshCount);
    for (auto& mesh : model.meshes) {
        if (!ReadMesh(reader, materialCount, mesh)) {
            return false;
        }
    }
    return reader.Remaining() == 0;
}

bool AddPayloadBytes(std::size_t& size, std::size_t fieldBytes)
{
    if (fieldBytes > MaxPayloadBytes - size) {
        return false;
    }
    size += fieldBytes;
    return true;
}

bool GetPayloadSize(const ModelData& model, std::size_t& size)
{
    if (model.materials.empty() || model.materials.size() > MaxMaterials ||
        model.meshes.empty() || model.meshes.size() > MaxMeshes) {
        return false;
    }
    size = 2 * CountBytes;
    for (const auto& material : model.materials) {
        if (!IsUtf8(material.name) || !IsUtf8(material.diffuseTexture) ||
            !ValidMaterialRanges(material) ||
            !AddPayloadBytes(size, MaterialBytes + material.name.size() + material.diffuseTexture.size())) {
            return false;
        }
    }
    for (const auto& mesh : model.meshes) {
        if (!IsUtf8(mesh.name) || mesh.materialIndex >= model.materials.size() ||
            mesh.vertices.empty() || mesh.vertices.size() > MaxPayloadBytes / VertexBytes ||
            mesh.indices.empty() || mesh.indices.size() > MaxPayloadBytes / IndexBytes ||
            mesh.indices.size() % 3 != 0 ||
            !AddPayloadBytes(size, MeshHeaderBytes + mesh.name.size() +
                                   mesh.vertices.size() * VertexBytes + mesh.indices.size() * IndexBytes)) {
            return false;
        }
    }
    return true;
}

void WriteMaterial(BinaryWriter& writer, const ModelMaterial& material)
{
    writer.WriteString(material.name);
    writer.WriteString(material.diffuseTexture);
    writer.WriteVec3(material.ambient);
    writer.WriteVec3(material.diffuse);
    writer.WriteVec3(material.specular);
    writer.WriteVec3(material.emissive);
    writer.WriteVec3(material.baseColor);
    writer.WriteFloat(material.shininess);
    writer.WriteFloat(material.opacity);
    writer.WriteFloat(material.metallic);
    writer.WriteFloat(material.roughness);
}

bool WritePayload(const ModelData& model, Bytes& bytes)
{
    BinaryWriter writer(bytes);
    writer.WriteUnsigned(static_cast<std::uint32_t>(model.materials.size()));
    for (const auto& material : model.materials) {
        WriteMaterial(writer, material);
    }

    writer.WriteUnsigned(static_cast<std::uint32_t>(model.meshes.size()));
    for (const auto& mesh : model.meshes) {
        writer.WriteString(mesh.name);
        writer.WriteUnsigned(mesh.materialIndex);
        writer.WriteUnsigned(static_cast<std::uint8_t>(mesh.hasUvs));
        writer.WriteUnsigned(static_cast<std::uint32_t>(mesh.vertices.size()));
        writer.WriteUnsigned(static_cast<std::uint32_t>(mesh.indices.size()));
        for (const auto& vertex : mesh.vertices) {
            writer.WriteVec3(vertex.position);
            writer.WriteVec3(vertex.normal);
            writer.WriteFloat(vertex.uv.x);
            writer.WriteFloat(vertex.uv.y);
        }
        for (const auto index : mesh.indices) {
            if (index >= mesh.vertices.size()) {
                return false;
            }
            writer.WriteUnsigned(index);
        }
    }
    return writer.Valid();
}

Bytes ReadFileBytes(const std::filesystem::path& path)
{
    const HANDLE handle = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_DELETE,
                                      nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (handle == INVALID_HANDLE_VALUE) {
        return {};
    }
    const std::unique_ptr<void, decltype(&CloseHandle)> file(handle, CloseHandle);
    LARGE_INTEGER size{};
    if (!GetFileSizeEx(handle, &size) || size.QuadPart < static_cast<LONGLONG>(HeaderBytes) ||
        size.QuadPart > static_cast<LONGLONG>(HeaderBytes + MaxPayloadBytes)) {
        return {};
    }
    Bytes bytes(static_cast<std::size_t>(size.QuadPart));
    const DWORD expected = static_cast<DWORD>(bytes.size());
    DWORD read{};
    if (!ReadFile(handle, bytes.data(), expected, &read, nullptr) || read != expected) {
        return {};
    }
    return bytes;
}

bool WriteBytes(HANDLE file, ByteView bytes)
{
    const DWORD expected = static_cast<DWORD>(bytes.size());
    DWORD written{};
    return WriteFile(file, bytes.data(), expected, &written, nullptr) && written == expected;
}

bool WriteAtomically(const std::filesystem::path& path, ByteView header, ByteView payload)
{
    static std::atomic<std::uint64_t> sequence{0};
    std::filesystem::path temporary;
    HANDLE file = INVALID_HANDLE_VALUE;
    for (int attempt = 0; attempt < 32; ++attempt) {
        temporary = path;
        temporary += L"." + std::to_wstring(GetCurrentProcessId()) +
                     L"." + std::to_wstring(++sequence) + L".tmp";
        // 다른 쓰기 작업이나 이전 실행의 파일을 덮어쓰지 않는다.
        file = CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW,
                           FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file != INVALID_HANDLE_VALUE) {
            break;
        }
        const DWORD error = GetLastError();
        if (error != ERROR_FILE_EXISTS && error != ERROR_ALREADY_EXISTS) {
            return false;
        }
    }
    if (file == INVALID_HANDLE_VALUE) {
        return false;
    }

    const bool written = WriteBytes(file, header) && WriteBytes(file, payload) && FlushFileBuffers(file);
    const bool closed = CloseHandle(file) != 0;
    // 같은 폴더에서 완성된 파일을 한 번에 교체한다.
    if (written && closed &&
        MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        return true;
    }
    DeleteFileW(temporary.c_str());
    return false;
}

} // namespace

bool ReadModelBinary(const std::filesystem::path& path, const ModelFingerprint& fingerprint,
                     ModelData& model)
try {
    const auto bytes = ReadFileBytes(path);
    if (bytes.empty()) {
        return false;
    }
    BinaryReader header(ByteView(bytes).first(HeaderBytes));
    const auto magic = header.Take(Magic.size());
    std::uint32_t version{};
    if (!std::equal(magic.begin(), magic.end(), Magic.begin(), Magic.end()) ||
        !header.ReadUnsigned(version) || version != Version) {
        return false;
    }
    const auto source = header.Take(fingerprint.size());
    std::uint32_t payloadBytes{};
    std::uint64_t checksum{};
    const auto payload = ByteView(bytes).subspan(HeaderBytes);
    if (!std::equal(source.begin(), source.end(), fingerprint.begin(), fingerprint.end()) ||
        !header.ReadUnsigned(payloadBytes) || payloadBytes != payload.size() ||
        !header.ReadUnsigned(checksum) || checksum != PayloadChecksum(payload)) {
        return false;
    }

    ModelData loaded;
    if (!ReadPayload(payload, loaded)) {
        return false;
    }
    model = std::move(loaded);
    return true;
}
catch (const std::bad_alloc&) {
    return false;
}

bool WriteModelBinary(const std::filesystem::path& path, const ModelFingerprint& fingerprint,
                      const ModelData& model)
try {
    std::size_t payloadBytes{};
    if (!GetPayloadSize(model, payloadBytes)) {
        return false;
    }
    Bytes payload;
    payload.reserve(payloadBytes);
    if (!WritePayload(model, payload) || payload.size() != payloadBytes) {
        return false;
    }
    Bytes header;
    header.reserve(HeaderBytes);
    header.insert(header.end(), Magic.begin(), Magic.end());
    BinaryWriter writer(header);
    writer.WriteUnsigned(Version);
    header.insert(header.end(), fingerprint.begin(), fingerprint.end());
    writer.WriteUnsigned(static_cast<std::uint32_t>(payloadBytes));
    writer.WriteUnsigned(PayloadChecksum(payload));
    return WriteAtomically(path, header, payload);
}
catch (const std::bad_alloc&) {
    return false;
}

} // namespace aegis
