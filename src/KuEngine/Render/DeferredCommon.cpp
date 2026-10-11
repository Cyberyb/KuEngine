#include "DeferredCommon.h"
#include <KuEngine/RHI/RHIDevice.h>
#include "ForwardGraphTargets.h"
#include <stdexcept>

namespace ku {
std::optional<GBufferShadingCode> encodeGBufferShadingModel(asset::ShadingModel model) noexcept
{
    if (model == asset::ShadingModel::PBR) return GBufferShadingCode::PBR;
    if (model == asset::ShadingModel::Unlit) return GBufferShadingCode::Unlit;
    return std::nullopt;
}
bool supportsDeferredGeometry(const ForwardPipelineKey& key) noexcept
{
    return encodeGBufferShadingModel(key.shadingModel).has_value()
        && (key.alphaMode == asset::AlphaMode::Opaque || key.alphaMode == asset::AlphaMode::Mask);
}
GBufferTargets makeGBufferTargets(VkFormat depthFormat, VkClearDepthStencilValue clearDepth)
{
    if (depthFormat == VK_FORMAT_UNDEFINED)
        throw std::invalid_argument("GBuffer requires a depth format");
    GBufferTargets result;
    for (auto& color : result.colors) {
        color.extent = ImageExtentDesc::swapchainRelative();
        color.format = gbufferFormat;
        color.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        color.aspect = VK_IMAGE_ASPECT_COLOR_BIT;
        color.initialContent = InitialContent::Cleared;
        color.clearValue.color = {{0.0f, 0.0f, 0.0f, 0.0f}};
    }
    result.depth.extent = ImageExtentDesc::swapchainRelative();
    result.depth.format = depthFormat;
    result.depth.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    result.depth.aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
    result.depth.initialContent = InitialContent::Cleared;
    result.depth.clearValue.depthStencil = clearDepth;
    return result;
}
void validateGBufferCapabilities(const VkPhysicalDeviceLimits& limits, VkFormatFeatureFlags features)
{
    constexpr auto required = VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT | VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT | VK_FORMAT_FEATURE_TRANSFER_SRC_BIT;
    if (limits.maxColorAttachments < gbufferAttachmentCount
        || limits.maxFragmentOutputAttachments < gbufferAttachmentCount)
        throw std::runtime_error("Deferred requires four color/fragment output attachments");
    if ((features & required) != required)
        throw std::runtime_error("Deferred requires RGBA16F color attachment and sampling support");
}
void validateGBufferFormats(const RHIDevice& device, VkFormat depthFormat)
{
    VkFormatProperties color{}, depth{};
    vkGetPhysicalDeviceFormatProperties(device.physicalDevice(), gbufferFormat, &color);
    validateGBufferCapabilities(device.properties().limits, color.optimalTilingFeatures);
    vkGetPhysicalDeviceFormatProperties(device.physicalDevice(), depthFormat, &depth);
    if (!(depth.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT))
        throw std::runtime_error("Deferred depth format does not support depth attachments");
}
ImageDesc makeDeferredSceneColor(const RenderContext& context)
{
    auto color = makeForwardGraphTargets(context).color;
    color.usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    VkFormatProperties properties{};
    vkGetPhysicalDeviceFormatProperties(context.device.physicalDevice(), color.format, &properties);
    if (!(properties.optimalTilingFeatures & VK_FORMAT_FEATURE_TRANSFER_SRC_BIT))
        throw std::runtime_error("Deferred temporary SceneColor must support transfer readback");
    return color;
}
} // namespace ku
