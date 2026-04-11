#include "RHIBuffer.h"
#include "RHIDevice.h"

namespace ku {

RHIBuffer::RHIBuffer(const RHIDevice& device, VmaMemoryUsage usage)
    : m_device(&device)
{
    // Default buffer: 1MB, host-visible
    VkBufferCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    info.size = 1024 * 1024;
    info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = usage;

    VK_CHECK(vmaCreateBuffer(m_device->allocator(), &info, &allocInfo,
                            &m_buffer, &m_allocation, nullptr));
}

RHIBuffer::~RHIBuffer()
{
    if (!m_device || m_buffer == VK_NULL_HANDLE || m_allocation == VK_NULL_HANDLE) {
        return;
    }

    vmaDestroyBuffer(m_device->allocator(), m_buffer, m_allocation);
}

void* RHIBuffer::map()
{
    void* data = nullptr;
    vmaMapMemory(m_device->allocator(), m_allocation, &data);
    return data;
}

void RHIBuffer::unmap()
{
    vmaUnmapMemory(m_device->allocator(), m_allocation);
}

void RHIBuffer::flush()
{
    vmaFlushAllocation(m_device->allocator(), m_allocation, 0, VK_WHOLE_SIZE);
}

void RHIBuffer::invalidate()
{
    vmaInvalidateAllocation(m_device->allocator(), m_allocation, 0, VK_WHOLE_SIZE);
}

} // namespace ku
