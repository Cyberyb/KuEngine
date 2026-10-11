#include "ForwardGraphTargets.h"

#include <KuEngine/RHI/RHIDevice.h>

#include <algorithm>
#include <stdexcept>

namespace ku {
bool isSrgbAttachmentFormat(VkFormat format) noexcept {
    return format == VK_FORMAT_R8G8B8A8_SRGB || format == VK_FORMAT_B8G8R8A8_SRGB
        || format == VK_FORMAT_A8B8G8R8_SRGB_PACK32;
}
bool needsManualDisplayEncoding(VkFormat format, bool encodeSrgb) noexcept {
    return encodeSrgb && !isSrgbAttachmentFormat(format);
}

ForwardGraphTargets makeForwardGraphTargets(const RenderContext& context)
{
    return makeForwardGraphTargets(
        sceneHdrFormat,
        context.depthFormat,
        context.clearColor,
        context.clearDepthStencil);
}

ForwardGraphTargets makeForwardGraphTargets(
    VkFormat colorFormat,
    VkFormat depthFormat,
    VkClearColorValue clearColor,
    VkClearDepthStencilValue clearDepthStencil)
{
    if (colorFormat == VK_FORMAT_UNDEFINED
        || depthFormat == VK_FORMAT_UNDEFINED) {
        throw std::invalid_argument(
            "Forward Graph targets require color and depth formats");
    }

    ForwardGraphTargets targets{};
    targets.color.extent = ImageExtentDesc::swapchainRelative();
    targets.color.format = colorFormat;
    targets.color.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
        | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    targets.color.aspect = VK_IMAGE_ASPECT_COLOR_BIT;
    targets.color.clearValue.color = clearColor;
    targets.color.initialContent = InitialContent::Cleared;

    targets.depth.extent = ImageExtentDesc::swapchainRelative();
    targets.depth.format = depthFormat;
    targets.depth.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    targets.depth.aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
    targets.depth.clearValue.depthStencil = clearDepthStencil;
    targets.depth.initialContent = InitialContent::Cleared;
    return targets;
}

void validateForwardGraphTargetFormats(
    const RHIDevice& device,
    VkFormat colorFormat,
    VkFormat depthFormat)
{
    VkFormatProperties colorProperties{};
    vkGetPhysicalDeviceFormatProperties(
        device.physicalDevice(), colorFormat, &colorProperties);
    validateSceneHdrCapabilities(colorProperties.optimalTilingFeatures);

    VkFormatProperties depthProperties{};
    vkGetPhysicalDeviceFormatProperties(
        device.physicalDevice(), depthFormat, &depthProperties);
    constexpr auto requiredDepth = VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_FORMAT_FEATURE_TRANSFER_SRC_BIT;
    if ((depthProperties.optimalTilingFeatures & requiredDepth) != requiredDepth) {
        throw std::runtime_error("Forward SceneDepth format does not support attachment/transfer");
    }
}
void validateSceneHdrCapabilities(VkFormatFeatureFlags features) {
    constexpr VkFormatFeatureFlags requiredColor =
        VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT
        | VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT
        | VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT | VK_FORMAT_FEATURE_TRANSFER_SRC_BIT;
    if ((features & requiredColor)
        != requiredColor) {
        throw std::runtime_error(
            "SceneColor RGBA16F requires attachment/blend/sampled/transfer support");
    }

}

ForwardDisplayUvTransform calculateForwardDisplayUvTransform(
    const ViewerPixelRect& sceneRect,
    VkExtent2D sourceExtent) noexcept
{
    if (sourceExtent.width == 0 || sourceExtent.height == 0) {
        return {};
    }
    const ViewerPixelRect fitted = fitViewerRectToExtent(
        sceneRect, {sourceExtent.width, sourceExtent.height});
    return ForwardDisplayUvTransform{
        {
            static_cast<float>(fitted.width)
                / static_cast<float>(sourceExtent.width),
            static_cast<float>(fitted.height)
                / static_cast<float>(sourceExtent.height),
        },
        {
            static_cast<float>(fitted.x)
                / static_cast<float>(sourceExtent.width),
            static_cast<float>(fitted.y)
                / static_cast<float>(sourceExtent.height),
        },
    };
}

bool forwardDisplaySourceChanged(
    uint64_t boundGeneration,
    VkImageView boundView,
    uint64_t resolvedGeneration,
    VkImageView resolvedView) noexcept
{
    return boundGeneration != resolvedGeneration || boundView != resolvedView;
}

} // namespace ku
