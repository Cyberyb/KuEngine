#pragma once

#include "RHICommon.h"
#include <vulkan/vulkan.h>

namespace ku {

class RHIDevice;

class CommandList {
public:
    CommandList(const RHIDevice& device, VkCommandPool pool);
    ~CommandList();

    void begin();
    void end();

    [[nodiscard]] VkCommandBuffer cmd() const { return m_cmd; }
    [[nodiscard]] operator VkCommandBuffer() const { return m_cmd; }

    // 屏障
    void imageBarrier(VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout,
                      VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage,
                      VkImageAspectFlags aspect = VK_IMAGE_ASPECT_COLOR_BIT);

    void copyBuffer(VkBuffer src, VkBuffer dst, VkDeviceSize size);

private:
    VkCommandBuffer m_cmd = VK_NULL_HANDLE;
    VkDevice        m_device = VK_NULL_HANDLE;
    bool            m_recording = false;
};

} // namespace ku
