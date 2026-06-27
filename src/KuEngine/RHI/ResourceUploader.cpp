#include "ResourceUploader.h"

#include "CommandList.h"
#include "RHIBuffer.h"
#include "RHIDevice.h"
#include "RHITexture.h"

#include <cstring>
#include <stdexcept>

namespace ku {

namespace {

RHIBuffer::CreateInfo stagingBufferInfo(VkDeviceSize size)
{
    RHIBuffer::CreateInfo info{};
    info.size = size;
    info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    info.memoryUsage = VMA_MEMORY_USAGE_AUTO;
    info.allocationFlags =
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
        VMA_ALLOCATION_CREATE_MAPPED_BIT;
    return info;
}

void writeStagingBuffer(RHIBuffer& staging, const void* data, VkDeviceSize size)
{
    if (data == nullptr || size == 0) {
        throw std::invalid_argument("ResourceUploader requires non-empty source data");
    }

    void* mapped = staging.map();
    if (mapped == nullptr) {
        throw std::runtime_error("ResourceUploader failed to map staging buffer");
    }

    std::memcpy(mapped, data, static_cast<size_t>(size));
    staging.flush();
    staging.unmap();
}

void submitImmediate(CommandList& cmd, RHIDevice& device)
{
    cmd.end();

    const VkCommandBuffer commandBuffer = cmd.cmd();
    VkSubmitInfo submitInfo{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    VK_CHECK(vkQueueSubmit(device.graphicsQueue(), 1, &submitInfo, VK_NULL_HANDLE));
    VK_CHECK(vkQueueWaitIdle(device.graphicsQueue()));
}

} // namespace

ResourceUploader::ResourceUploader(RHIDevice& device)
    : m_device(&device)
    , m_commandPool(device.createCommandPool())
{
}

ResourceUploader::~ResourceUploader()
{
    if (m_commandPool != VK_NULL_HANDLE && m_device != nullptr) {
        vkDestroyCommandPool(m_device->device(), m_commandPool, nullptr);
    }
}

void ResourceUploader::uploadBuffer(
    const void* data,
    VkDeviceSize size,
    RHIBuffer& destination)
{
    if (size > destination.size()) {
        throw std::invalid_argument("ResourceUploader source data exceeds destination buffer size");
    }

    RHIBuffer staging(*m_device, stagingBufferInfo(size));
    writeStagingBuffer(staging, data, size);

    CommandList commandList(*m_device, m_commandPool);
    commandList.begin();
    commandList.copyBuffer(staging.buffer(), destination.buffer(), size);
    submitImmediate(commandList, *m_device);
}

void ResourceUploader::uploadTexture2D(
    const void* data,
    VkDeviceSize size,
    uint32_t width,
    uint32_t height,
    RHITexture& destination,
    VkImageLayout finalLayout)
{
    if (width == 0 || height == 0) {
        throw std::invalid_argument("ResourceUploader requires a non-zero texture extent");
    }
    if (width != destination.width() || height != destination.height()) {
        throw std::invalid_argument("ResourceUploader source extent does not match destination texture");
    }

    RHIBuffer staging(*m_device, stagingBufferInfo(size));
    writeStagingBuffer(staging, data, size);

    CommandList commandList(*m_device, m_commandPool);
    commandList.begin();
    commandList.imageBarrier(
        destination.image(),
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT);
    commandList.copyBufferToImage(
        staging.buffer(),
        destination.image(),
        width,
        height);
    commandList.imageBarrier(
        destination.image(),
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        finalLayout,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
    submitImmediate(commandList, *m_device);
}

} // namespace ku
