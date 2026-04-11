#include "RHIBuffer.h"
#include "RHIDevice.h"

namespace ku {

RHIBuffer::RHIBuffer(const RHIDevice& device, VkDeviceSize size,
    VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage)
    : m_size(size), m_allocator(device.allocator()) {
    
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = memoryUsage;
    allocInfo.preferredFlags = 0;

    VK_CHECK(vmaCreateBuffer(m_allocator, &bufferInfo, &allocInfo, &m_buffer, &m_allocation, nullptr));
}

RHIBuffer::~RHIBuffer() {
    if (m_buffer != VK_NULL_HANDLE && m_allocator != VK_NULL_HANDLE) {
        vmaDestroyBuffer(m_allocator, m_buffer, m_allocation);
    }
}

RHIBuffer::RHIBuffer(RHIBuffer&& other) noexcept
    : m_buffer(other.m_buffer), m_allocation(other.m_allocation),
      m_size(other.m_size), m_allocator(other.m_allocator) {
    other.m_buffer = VK_NULL_HANDLE;
    other.m_allocation = VK_NULL_HANDLE;
}

RHIBuffer& RHIBuffer::operator=(RHIBuffer&& other) noexcept {
    if (this != &other) {
        if (m_buffer != VK_NULL_HANDLE) {
            vmaDestroyBuffer(m_allocator, m_buffer, m_allocation);
        }
        m_buffer = other.m_buffer;
        m_allocation = other.m_allocation;
        m_size = other.m_size;
        m_allocator = other.m_allocator;
        other.m_buffer = VK_NULL_HANDLE;
        other.m_allocation = VK_NULL_HANDLE;
    }
    return *this;
}

void* RHIBuffer::mapMemory() {
    void* data = nullptr;
    vmaMapMemory(m_allocator, m_allocation, &data);
    return data;
}

void RHIBuffer::unmapMemory() {
    vmaUnmapMemory(m_allocator, m_allocation);
}

void RHIBuffer::uploadData(const void* data, VkDeviceSize size, VkDeviceSize offset) {
    void* mapped = mapMemory();
    std::memcpy(static_cast<char*>(mapped) + offset, data, size);
    unmapMemory();
}

} // namespace ku
