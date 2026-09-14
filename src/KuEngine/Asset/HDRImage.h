// KuEngine HDR CPU asset: validated RGBA float pixels without Vulkan ownership.
#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace ku::asset {

struct HDRImageData {
    uint32_t width = 0;
    uint32_t height = 0;
    std::vector<float> rgba32f;

    [[nodiscard]] bool valid() const;
};

[[nodiscard]] bool checkedHdrElementCount(
    uint64_t width,
    uint64_t height,
    size_t& outElementCount);

class HDRImageLoader {
public:
    [[nodiscard]] static bool loadFromFile(
        const std::filesystem::path& path,
        HDRImageData& outImage,
        std::string* errorMessage = nullptr);
};

} // namespace ku::asset
