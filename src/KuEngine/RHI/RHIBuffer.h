#pragma once

#include "RHICommon.h"
#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

namespace ku {

class RHIDevice;

class RHIBuffer {
public:
    RHIBuffer() = default;
    RHIBuffer(const RHIDevice& device, VkDeviceSize size, 
              VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage);
    ~RHIBuffer();

    // 禁止拷贝
    RHIBuffer(const RHIBuffer&) = delete;
    RHIBuffer& operator=(const RHIBuffer&) = delete;

    // 支持移动
    RHIBuffer(RHIBuffer&& other) noexcept;
    RHIBuffer& operator=(RHIBuffer&& other) noexcept;

    void uploadData(const void* data, VkDeviceSize size, VkDeviceSize offset = 0);
    [[nodiscard]] void* mapMemory();
    void unmapMemory();

    [[nodiscard]] VkBuffer buffer() const { return m_buffer; }
    [[nodiscard]] VkDeviceSize size() const { return m_size; }

private:
    VkBuffer     m_buffer = VK_NULL_HANDLE;
    VmaAllocation m_allocation = VK_NULL_HANDLE;
    VkDeviceSize m_size = 0;
    VmaAllocator m_allocator = VK_NULL_HANDLE;
};

} // namespace ku
