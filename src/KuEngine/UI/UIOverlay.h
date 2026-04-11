#pragma once

#include <vulkan/vulkan.h>
#include <string_view>
#include <vector>
#include <memory>

struct GLFWwindow;

namespace ku {

class RHIDevice;

class UIOverlay {
public:
    UIOverlay(
        const RHIDevice& device,
        ::GLFWwindow* window,
        VkInstance instance,
        VkFormat imageFormat,
        uint32_t imageCount);
    ~UIOverlay();

    void newFrame();
    void render(VkCommandBuffer cmd, VkImageView imageView, VkImageLayout imageLayout);
    void drawFPSPanel(float fps, float deltaTime);
    void onSwapChainRecreated(uint32_t imageCount);

private:
    void init(::GLFWwindow* window, VkInstance instance, VkFormat imageFormat, uint32_t imageCount);
    [[nodiscard]] VkDescriptorPool createDescriptorPool() const;
    static void checkVkResult(VkResult result);

    const RHIDevice* m_device = nullptr;
    VkDescriptorPool m_descriptorPool = VK_NULL_HANDLE;
    bool m_initialized = false;
};

} // namespace ku
