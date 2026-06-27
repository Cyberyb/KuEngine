// KuEngine 命令列表模块：封装 Vulkan 命令缓冲的分配、录制与常用渲染命令。
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
    void copyBufferToImage(VkBuffer src, VkImage dst, uint32_t width, uint32_t height);

private:
    VkCommandBuffer m_cmd = VK_NULL_HANDLE;
    VkDevice        m_device = VK_NULL_HANDLE;
    bool            m_recording = false;
};

} // namespace ku
