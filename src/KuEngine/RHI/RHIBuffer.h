#pragma once

#include "RHICommon.h"
#include <vulkan/vulkan.h>
#include <vma/vk_mem_alloc.h>

namespace ku {

class RHIDevice;

class RHIBuffer {
public:
    RHIBuffer(const RHIDevice& device, VmaMemoryUsage usage);
    ~RHIBuffer();

    [[nodiscard]] VkBuffer buffer() const { return m_buffer; }
    [[nodiscard]] VmaAllocation allocation() const { return m_allocation; }

    void* map();
    void unmap();
    void flush();
    void invalidate();

private:
    const RHIDevice* m_device = nullptr;
    VkBuffer         m_buffer = VK_NULL_HANDLE;
    VmaAllocation    m_allocation = VK_NULL_HANDLE;
};

} // namespace ku
