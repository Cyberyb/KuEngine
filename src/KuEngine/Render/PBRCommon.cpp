#include "PBRCommon.h"

#include <algorithm>
#include <cctype>
#include <limits>
#include <stdexcept>

#include <KuEngine/Asset/AssetConfig.h>
#include <KuEngine/Core/Log.h>

namespace ku {

namespace {

constexpr VkFormat kDefaultSrgbFormat = VK_FORMAT_R8G8B8A8_SRGB;
constexpr VkFormat kDefaultLinearFormat = VK_FORMAT_R8G8B8A8_UNORM;

} // namespace

VkDeviceSize alignedUniformBufferStride(
    VkDeviceSize elementSize,
    VkDeviceSize minimumAlignment)
{
    if (minimumAlignment <= 1) {
        return elementSize;
    }

    const VkDeviceSize remainder = elementSize % minimumAlignment;
    if (remainder == 0) {
        return elementSize;
    }

    const VkDeviceSize padding = minimumAlignment - remainder;
    if (elementSize > std::numeric_limits<VkDeviceSize>::max() - padding) {
        throw std::overflow_error("Uniform buffer stride alignment overflow");
    }
    return elementSize + padding;
}

std::string toLower(std::string_view text)
{
    std::string out;
    out.reserve(text.size());
    for (unsigned char ch : text) {
        out.push_back(static_cast<char>(std::tolower(ch)));
    }
    return out;
}

bool isDisabledSource(const std::string& sourceLower)
{
    return sourceLower == "none" || sourceLower == "disabled" || sourceLower == "off";
}

float clampTexCoordSet(uint32_t texCoord)
{
    return texCoord == 0 ? 0.0f : 1.0f;
}

const asset::TextureData* resolveGltfTexture(
    std::string_view source,
    const asset::MaterialData& material)
{
    const std::string sourceLower = toLower(source);
    if (isDisabledSource(sourceLower)) {
        return nullptr;
    }

    const std::string prefix = "gltf:";
    if (sourceLower.rfind(prefix, 0) != 0) {
        return nullptr;
    }

    std::string_view rest = std::string_view(source).substr(prefix.size());
    size_t start = 0;
    while (start < rest.size()) {
        const size_t sep = rest.find('|', start);
        const size_t end = (sep == std::string_view::npos) ? rest.size() : sep;
        std::string token = toLower(rest.substr(start, end - start));

        if (token == "basecolortexture") {
            return &material.baseColorTexture;
        }
        if (token == "normaltexture") {
            return &material.normalTexture;
        }
        if (token == "metallicroughnesstexture" || token == "occlusiontexture" || token == "ormtexture") {
            return &material.ormTexture;
        }
        if (token == "emissivetexture") {
            return &material.emissiveTexture;
        }

        if (sep == std::string_view::npos) {
            break;
        }
        start = sep + 1;
    }

    return nullptr;
}

VkFormat formatForBinding(
    const asset::MaterialConfig::TextureBindingConfig& binding,
    VkFormat defaultFormat,
    std::string_view bindingName)
{
    if (!binding.hasColorSpace) {
        return defaultFormat;
    }

    const std::string colorSpace = toLower(binding.colorSpace);
    if (colorSpace == "srgb") {
        if (bindingName == "normal" || bindingName == "orm") {
            KU_WARN("PBRCommon: {} binding uses sRGB, forcing Linear", std::string(bindingName));
            return kDefaultLinearFormat;
        }
        return kDefaultSrgbFormat;
    }
    if (colorSpace == "linear") {
        if (bindingName == "baseColor" || bindingName == "emissive") {
            KU_WARN("PBRCommon: {} binding uses Linear, forcing sRGB", std::string(bindingName));
            return kDefaultSrgbFormat;
        }
        return kDefaultLinearFormat;
    }

    KU_WARN(
        "PBRCommon: {} binding uses unknown colorSpace '{}', using default format",
        std::string(bindingName),
        binding.colorSpace);
    return defaultFormat;
}

float alphaModeToFlag(const asset::MaterialConfig& config)
{
    const std::string alphaMode = toLower(config.alphaMode);
    if (alphaMode == "mask") {
        return 1.0f;
    }
    if (alphaMode == "blend") {
        return 2.0f;
    }
    return 0.0f;
}

} // namespace ku
