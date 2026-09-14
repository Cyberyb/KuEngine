#include "AssetPath.h"

#include <algorithm>
#include <cctype>
#include <system_error>

namespace ku::asset {

std::filesystem::path normalizeAssetPath(
    const std::filesystem::path& path)
{
    if (path.empty()) {
        return {};
    }

    std::error_code error;
    std::filesystem::path absolutePath = path.is_absolute()
        ? path
        : std::filesystem::absolute(path, error);
    if (error) {
        error.clear();
        absolutePath = path;
    }

    const std::filesystem::path canonicalPath =
        std::filesystem::weakly_canonical(absolutePath, error);
    if (!error) {
        return canonicalPath.lexically_normal();
    }
    return absolutePath.lexically_normal();
}

std::filesystem::path resolveAssetPath(
    const std::filesystem::path& root,
    const std::filesystem::path& path)
{
    if (path.empty()) {
        return {};
    }
    return normalizeAssetPath(path.is_absolute() ? path : root / path);
}

std::string assetPathKey(const std::filesystem::path& path)
{
    std::string key = normalizeAssetPath(path).generic_string();
#ifdef _WIN32
    std::transform(
        key.begin(),
        key.end(),
        key.begin(),
        [](unsigned char value) {
            return static_cast<char>(std::tolower(value));
        });
#endif
    return key;
}

} // namespace ku::asset
