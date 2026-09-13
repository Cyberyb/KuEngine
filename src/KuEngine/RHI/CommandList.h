// KuEngine 命令列表模块：封装 Vulkan 命令缓冲的分配、录制与常用渲染命令。
#pragma once

#include "RHICommon.h"
#include "../Core/FrameStatistics.h"
#include <cstdint>
#include <vulkan/vulkan.h>

namespace ku {

class RHIDevice;

class CommandList {
public:
    CommandList(const RHIDevice& device, VkCommandPool pool);
    ~CommandList();

    void begin();
    void end();
    [[nodiscard]] GpuTimeSample collectGpuTime();
    [[nodiscard]] bool gpuTimingSupported() const noexcept
    {
        return m_timestampQueryPool != VK_NULL_HANDLE;
    }

    [[nodiscard]] VkCommandBuffer cmd() const { return m_cmd; }
    [[nodiscard]] operator VkCommandBuffer() const { return m_cmd; }

    // 屏障
    void imageBarrier(VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout,
                      VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage,
                      VkImageAspectFlags aspect = VK_IMAGE_ASPECT_COLOR_BIT);

    void copyBuffer(VkBuffer src, VkBuffer dst, VkDeviceSize size);
    void copyBufferToImage(VkBuffer src, VkImage dst, uint32_t width, uint32_t height);

    void draw(
        uint32_t vertexCount,
        uint32_t instanceCount = 1,
        uint32_t firstVertex = 0,
        uint32_t firstInstance = 0);
    void drawIndexed(
        uint32_t indexCount,
        uint32_t instanceCount = 1,
        uint32_t firstIndex = 0,
        int32_t vertexOffset = 0,
        uint32_t firstInstance = 0);

    [[nodiscard]] const CommandListStatistics& statistics() const noexcept
    {
        return m_statistics.statistics();
    }

private:
    VkCommandBuffer m_cmd = VK_NULL_HANDLE;
    VkDevice        m_device = VK_NULL_HANDLE;
    VkQueryPool     m_timestampQueryPool = VK_NULL_HANDLE;
    float           m_timestampPeriod = 0.0f;
    uint32_t        m_timestampValidBits = 0;
    bool            m_recording = false;
    bool            m_timestampPending = false;
    CommandStatisticsAccumulator m_statistics;
};

} // namespace ku
