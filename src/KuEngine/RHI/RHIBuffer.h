// KuEngine RHI 缓冲模块：封装 Vulkan/VMA 缓冲的创建、映射、刷新与资源释放。
#pragma once

#include "RHICommon.h"
#include <vulkan/vulkan.h>
#include <vma/vk_mem_alloc.h>

namespace ku {

class RHIDevice;

class RHIBuffer {
public:
    struct CreateInfo {
        VkDeviceSize size = 1024 * 1024;
        VkBufferUsageFlags usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        VmaMemoryUsage memoryUsage = VMA_MEMORY_USAGE_AUTO;
        VmaAllocationCreateFlags allocationFlags = 0;
    };

    RHIBuffer(const RHIDevice& device, VmaMemoryUsage usage);
    RHIBuffer(const RHIDevice& device, const CreateInfo& info);
    ~RHIBuffer();

    [[nodiscard]] VkBuffer buffer() const { return m_buffer; }
    [[nodiscard]] VmaAllocation allocation() const { return m_allocation; }
    [[nodiscard]] VkDeviceSize size() const { return m_size; }

    void* map();
    void unmap();
    void flush();
    void invalidate();

private:
    const RHIDevice* m_device = nullptr;
    VkBuffer         m_buffer = VK_NULL_HANDLE;
    VmaAllocation    m_allocation = VK_NULL_HANDLE;
    VkDeviceSize     m_size = 0;
};

} // namespace ku
