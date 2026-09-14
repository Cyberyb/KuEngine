// KuEngine UI 覆盖层模块：封装 ImGui 的 GLFW/Vulkan 接入、逐帧绘制与交换链适配。
#pragma once

#include "../Core/FrameStatistics.h"
#include "SidebarState.h"

#include <vulkan/vulkan.h>
#include <cstdint>
#include <functional>
#include <optional>

struct GLFWwindow;

namespace ku {

class RHIDevice;

struct UIFrameStatistics {
    float fps = 0.0f;
    float frameTimeMilliseconds = 0.0f;
    std::optional<CompletedFrameStatistics> completedFrame;
    GpuTimeStatus gpuStatusBeforeFirstCompletedFrame = GpuTimeStatus::Waiting;
};

class UIOverlay {
public:
    using SidebarContent = std::function<void()>;

    UIOverlay(
        const RHIDevice& device,
        ::GLFWwindow* window,
        VkInstance instance,
        VkFormat imageFormat,
        uint32_t imageCount,
        VkFormat depthFormat = VK_FORMAT_UNDEFINED);
    ~UIOverlay();

    void newFrame();
    void render(VkCommandBuffer cmd, VkImageView imageView, VkImageLayout imageLayout);
    [[nodiscard]] SidebarFrameLayout describeSidebarFrame(
        float logicalWindowWidth,
        float logicalWindowHeight,
        bool showStats) const noexcept;
    void setSidebarExpanded(bool expanded) noexcept
    {
        if (expanded) {
            m_sidebarState.expand();
        } else {
            m_sidebarState.collapse();
        }
    }
    void drawSidebar(
        const SidebarFrameLayout& sidebarFrame,
        const UIFrameStatistics& stats,
        const SidebarContent& parameterContent,
        const SidebarContent& renderGraphContent);
    void onSwapChainRecreated(uint32_t imageCount);

private:
    void init(
        ::GLFWwindow* window,
        VkInstance instance,
        VkFormat imageFormat,
        uint32_t imageCount,
        VkFormat depthFormat);
    [[nodiscard]] VkDescriptorPool createDescriptorPool() const;
    static void checkVkResult(VkResult result);
    void drawStatisticsContent(
        const UIFrameStatistics& stats,
        bool compact) const;

    const RHIDevice* m_device = nullptr;
    VkDescriptorPool m_descriptorPool = VK_NULL_HANDLE;
    bool m_initialized = false;
    SidebarState m_sidebarState;
    SidebarLayoutPolicy m_sidebarLayoutPolicy;
};

} // namespace ku
