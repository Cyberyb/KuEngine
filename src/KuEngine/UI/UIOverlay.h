#pragma once

#include <vulkan/vulkan.h>
#include <string_view>
#include <vector>
#include <memory>

struct ImGuiContext;

namespace ku {

class RHIDevice;
class SwapChain;
class RHIPipeline;
class RHIShader;
class RHIBuffer;

class UIOverlay {
public:
    UIOverlay(const RHIDevice& device, VkFormat imageFormat);
    ~UIOverlay();

    void newFrame();
    void render(VkCommandBuffer cmd, VkImageView imageView, VkImageLayout imageLayout);

    // 辅助：渲染 FPS 信息面板
    void drawFPSPanel(float fps, float deltaTime);

private:
    void init(VkFormat imageFormat);

    // Vulkan 资源
    struct FrameResources {
        VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
    };

    std::vector<FrameResources> m_frameResources;
    const RHIDevice* m_device = nullptr;
    ImGuiContext*     m_imguiContext = nullptr;
    bool              m_initialized = false;
};

} // namespace ku
