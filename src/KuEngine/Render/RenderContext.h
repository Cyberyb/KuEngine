// KuEngine 渲染上下文：向 Pass 提供由公共 Runtime 决定的格式、帧配置与设备能力。
#pragma once

#include <cstdint>
#include <string_view>

#include <vulkan/vulkan.h>

namespace ku {

class RHIDevice;

namespace runtime_resource {

inline constexpr std::string_view swapChainColor = "SwapChainColor";
inline constexpr std::string_view sceneDepth = "SceneDepth";

} // namespace runtime_resource

struct RenderContext {
    RHIDevice& device;
    VkFormat colorFormat = VK_FORMAT_UNDEFINED;
    VkFormat depthFormat = VK_FORMAT_UNDEFINED;
    VkExtent2D initialExtent{0, 0};
    uint32_t framesInFlight = 1;
    VkCompareOp depthCompareOp = VK_COMPARE_OP_LESS;
    const VkPhysicalDeviceProperties& deviceProperties;
    const VkPhysicalDeviceFeatures& deviceFeatures;
    const VkPhysicalDeviceVulkan13Features& deviceFeatures13;

    [[nodiscard]] bool hasDepth() const
    {
        return depthFormat != VK_FORMAT_UNDEFINED;
    }
};

} // namespace ku
