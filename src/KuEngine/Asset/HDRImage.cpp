#include "HDRImage.h"

#include <limits>
#include <memory>
#include <utility>

#include <stb_image.h>

namespace ku::asset {

namespace {

void setError(std::string* destination, std::string message)
{
    if (destination != nullptr) {
        *destination = std::move(message);
    }
}

} // namespace

bool checkedHdrElementCount(
    uint64_t width,
    uint64_t height,
    size_t& outElementCount)
{
    outElementCount = 0;
    if (width == 0 || height == 0) {
        return false;
    }
    constexpr uint64_t channels = 4;
    constexpr uint64_t maximum =
        static_cast<uint64_t>(std::numeric_limits<size_t>::max());
    if (width > maximum / height) {
        return false;
    }
    const uint64_t pixels = width * height;
    if (pixels > maximum / channels) {
        return false;
    }
    outElementCount = static_cast<size_t>(pixels * channels);
    return true;
}

bool HDRImageData::valid() const
{
    size_t expectedElements = 0;
    return checkedHdrElementCount(width, height, expectedElements)
        && rgba32f.size() == expectedElements;
}

bool HDRImageLoader::loadFromFile(
    const std::filesystem::path& path,
    HDRImageData& outImage,
    std::string* errorMessage)
{
    if (errorMessage != nullptr) {
        errorMessage->clear();
    }

    // stbi_loadf also accepts LDR formats by converting them to floats.  This
    // loader is the HDR CPU boundary, so reject those inputs explicitly and
    // leave the caller's published image untouched on failure.
    if (path.empty() || stbi_is_hdr(path.string().c_str()) == 0) {
        setError(errorMessage, "Input is not a supported HDR image: " + path.string());
        return false;
    }

    int width = 0;
    int height = 0;
    int sourceChannels = 0;
    float* rawPixels = stbi_loadf(
        path.string().c_str(),
        &width,
        &height,
        &sourceChannels,
        4);
    const std::unique_ptr<float, decltype(&stbi_image_free)> pixels(
        rawPixels,
        &stbi_image_free);
    if (rawPixels == nullptr) {
        const char* reason = stbi_failure_reason();
        setError(
            errorMessage,
            "Failed to decode HDR image: " + path.string()
                + (reason != nullptr ? " (" + std::string(reason) + ")" : ""));
        return false;
    }
    if (width <= 0 || height <= 0) {
        setError(errorMessage, "HDR image dimensions are invalid");
        return false;
    }

    size_t elementCount = 0;
    if (!checkedHdrElementCount(
            static_cast<uint64_t>(width),
            static_cast<uint64_t>(height),
            elementCount)) {
        setError(errorMessage, "HDR image dimensions overflow addressable storage");
        return false;
    }

    HDRImageData candidate{};
    candidate.width = static_cast<uint32_t>(width);
    candidate.height = static_cast<uint32_t>(height);
    candidate.rgba32f.assign(rawPixels, rawPixels + elementCount);
    if (!candidate.valid()) {
        setError(errorMessage, "Decoded HDR image payload is inconsistent");
        return false;
    }

    outImage = std::move(candidate);
    return true;
}

} // namespace ku::asset
