// KuEngine asset path helpers: resolve portable asset references to stable paths.
#pragma once

#include <filesystem>
#include <string>

namespace ku::asset {

[[nodiscard]] std::filesystem::path normalizeAssetPath(
    const std::filesystem::path& path);

[[nodiscard]] std::filesystem::path resolveAssetPath(
    const std::filesystem::path& root,
    const std::filesystem::path& path);

[[nodiscard]] std::string assetPathKey(
    const std::filesystem::path& path);

} // namespace ku::asset
