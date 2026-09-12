// KuEngine UI 覆盖层模块：封装 ImGui 的 GLFW/Vulkan 接入、逐帧绘制与交换链适配。
#pragma once

#include <vulkan/vulkan.h>
#include <cstdint>
#include <string_view>
#include <vector>
#include <memory>

struct GLFWwindow;

namespace ku {

class RHIDevice;

struct UIFrameStatistics {
    float fps = 0.0f;
    float frameTimeMilliseconds = 0.0f;
    float cpuTimeMilliseconds = 0.0f;
    double gpuTimeMilliseconds = 0.0;
    uint64_t drawCalls = 0;
    uint64_t submittedVertices = 0;
    bool cpuTimeValid = false;
    bool gpuTimeValid = false;
};

class UIOverlay {
public:
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
    void drawStats(const UIFrameStatistics& stats);
    void drawFPSPanel(const UIFrameStatistics& stats);
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

    const RHIDevice* m_device = nullptr;
    VkDescriptorPool m_descriptorPool = VK_NULL_HANDLE;
    bool m_initialized = false;
};

} // namespace ku
