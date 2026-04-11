#pragma once

#include <vulkan/vulkan.h>
#include <string_view>
#include <vector>
#include <memory>

namespace ku {

class RHIDevice;

class UIOverlay {
public:
    UIOverlay(const RHIDevice& device, VkFormat imageFormat);
    ~UIOverlay();

    void newFrame();
    void render(VkCommandBuffer cmd, VkImageView imageView, VkImageLayout imageLayout);
    void drawFPSPanel(float fps, float deltaTime);

private:
    void init(VkFormat imageFormat);

    struct FrameResources {
        VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
    };

    std::vector<FrameResources> m_frameResources;
    const RHIDevice* m_device = nullptr;
    bool m_initialized = false;
};

} // namespace ku
