#include "RHIBuffer.h"
#include "RHIDevice.h"

namespace ku {

RHIBuffer::RHIBuffer(const RHIDevice& device, VmaMemoryUsage usage)
    : RHIBuffer(device, CreateInfo{
        1024 * 1024,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        usage,
        0,
    })
{}

RHIBuffer::RHIBuffer(const RHIDevice& device, const CreateInfo& info)
    : m_device(&device)
    , m_size(info.size)
{
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = info.size;
    bufferInfo.usage = info.usage;

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = info.memoryUsage;
    allocInfo.flags = info.allocationFlags;

    VK_CHECK(vmaCreateBuffer(
        m_device->allocator(),
        &bufferInfo,
        &allocInfo,
        &m_buffer,
        &m_allocation,
        nullptr));
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
