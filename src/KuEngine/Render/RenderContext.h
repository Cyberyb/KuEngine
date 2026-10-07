// KuEngine 渲染上下文：向 Pass 提供由公共 Runtime 决定的格式、帧配置与设备能力。
#pragma once

#include <cstdint>
#include <string_view>

#include <vulkan/vulkan.h>

#include "RenderGraph.h"

namespace ku {

class RHIDevice;

namespace runtime_resource {

inline constexpr std::string_view swapChainColor = "SwapChainColor";

} // namespace runtime_resource

struct RenderContext {
    RHIDevice& device;
    VkFormat colorFormat = VK_FORMAT_UNDEFINED;
    VkFormat depthFormat = VK_FORMAT_UNDEFINED;
    VkExtent2D initialExtent{0, 0};
    uint32_t framesInFlight = 1;
    VkCompareOp depthCompareOp = VK_COMPARE_OP_LESS;
    VkClearColorValue clearColor{};
    VkClearDepthStencilValue clearDepthStencil{1.0f, 0};
    const VkPhysicalDeviceProperties& deviceProperties;
    const VkPhysicalDeviceFeatures& deviceFeatures;
    const VkPhysicalDeviceVulkan13Features& deviceFeatures13;

    [[nodiscard]] bool hasDepth() const
    {
        return depthFormat != VK_FORMAT_UNDEFINED;
    }
};

[[nodiscard]] inline ImageDesc runtimeColorImageDesc(
    const RenderContext& context)
{
    ImageDesc desc{};
    desc.extent = ImageExtentDesc::swapchainRelative();
    desc.format = context.colorFormat;
    desc.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    desc.aspect = VK_IMAGE_ASPECT_COLOR_BIT;
    desc.initialContent = InitialContent::Undefined;
    return desc;
}

} // namespace ku
