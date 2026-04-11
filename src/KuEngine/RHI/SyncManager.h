#pragma once

#include "RHICommon.h"
#include <vulkan/vulkan.h>
#include <vector>

namespace ku {

class RHIDevice;

struct FrameSync {
    VkSemaphore imageAvailable = VK_NULL_HANDLE;
    VkSemaphore renderFinished = VK_NULL_HANDLE;
    VkFence     inFlight = VK_NULL_HANDLE;
};

class SyncManager {
public:
    SyncManager(const RHIDevice& device, uint32_t framesInFlight = 2);
    ~SyncManager();

    void waitForFrame(uint32_t frame);
    void submit(uint32_t frame, VkQueue queue, std::span<VkCommandBuffer> cmds);
    bool present(uint32_t frame, VkQueue queue, VkSwapchainKHR swapChain, uint32_t imageIndex);

    [[nodiscard]] uint32_t currentFrame() const { return m_currentFrame; }
    [[nodiscard]] uint32_t currentImage() const { return m_currentImage; }
    [[nodiscard]] FrameSync& frameSync(uint32_t frame) { return m_frames[frame]; }
    [[nodiscard]] void incrementFrame() { m_currentFrame = (m_currentFrame + 1) % m_frames.size(); }

    void setCurrentImage(uint32_t img) { m_currentImage = img; }

private:
    std::vector<FrameSync> m_frames;
    uint32_t m_currentFrame = 0;
    uint32_t m_currentImage = 0;

    const RHIDevice* m_device = nullptr;
};

} // namespace ku
