#pragma once

#include <array>
#include <cstdint>
#include <filesystem>

namespace aegis {

struct ModelData;

using ModelFingerprint = std::array<std::uint8_t, 32>;

bool ReadModelBinary(const std::filesystem::path& path, const ModelFingerprint& fingerprint,
                     ModelData& model);
bool WriteModelBinary(const std::filesystem::path& path, const ModelFingerprint& fingerprint,
                      const ModelData& model);

} // namespace aegis
