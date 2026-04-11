#include "CommandList.h"
#include "RHIDevice.h"

namespace ku {

CommandList::CommandList(const RHIDevice& device, VkCommandPool pool)
    : m_device(device.device()), m_recording(false)
{
    VkCommandBufferAllocateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    info.commandPool = pool;
    info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    info.commandBufferCount = 1;
    VK_CHECK(vkAllocateCommandBuffers(m_device, &info, &m_cmd));
}

CommandList::~CommandList()
{
    if (m_cmd) {} // freed with pool
}

void CommandList::begin()
{
    VkCommandBufferBeginInfo info{};
    info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    VK_CHECK(vkBeginCommandBuffer(m_cmd, &info));
    m_recording = true;
}

void CommandList::end()
{
    VK_CHECK(vkEndCommandBuffer(m_cmd));
    m_recording = false;
}

void CommandList::imageBarrier(VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout,
                               VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage,
                               VkImageAspectFlags aspect)
{
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = aspect;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    vkCmdPipelineBarrier(m_cmd, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
}

void CommandList::copyBuffer(VkBuffer src, VkBuffer dst, VkDeviceSize size)
{
    VkBufferCopy region{};
    region.size = size;
    vkCmdCopyBuffer(m_cmd, src, dst, 1, &region);
}

} // namespace ku
