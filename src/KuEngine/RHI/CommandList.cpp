#include "CommandList.h"
#include "RHIDevice.h"

#include <array>

namespace ku {

namespace {

VkAccessFlags accessMaskForLayout(VkImageLayout layout, bool isDst)
{
    switch (layout) {
        case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
            return isDst ? VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
                         : VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        case VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL:
        case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
            return VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT
                 | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
            return VK_ACCESS_SHADER_READ_BIT;
        case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
            return VK_ACCESS_TRANSFER_WRITE_BIT;
        case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
            return VK_ACCESS_TRANSFER_READ_BIT;
        case VK_IMAGE_LAYOUT_GENERAL:
            return VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
        case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
        case VK_IMAGE_LAYOUT_UNDEFINED:
        default:
            return 0;
    }
}

} // namespace

CommandList::CommandList(const RHIDevice& device, VkCommandPool pool)
    : m_device(device.device()),
      m_timestampPeriod(device.properties().limits.timestampPeriod),
      m_timestampValidBits(device.graphicsTimestampValidBits()),
      m_recording(false)
{
    VkCommandBufferAllocateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    info.commandPool = pool;
    info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    info.commandBufferCount = 1;
    VK_CHECK(vkAllocateCommandBuffers(m_device, &info, &m_cmd));

    if (m_timestampValidBits > 0 && m_timestampPeriod > 0.0f) {
        VkQueryPoolCreateInfo queryInfo{};
        queryInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
        queryInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
        queryInfo.queryCount = 2;
        VK_CHECK(vkCreateQueryPool(
            m_device,
            &queryInfo,
            nullptr,
            &m_timestampQueryPool));
    }
}

CommandList::~CommandList()
{
    if (m_timestampQueryPool != VK_NULL_HANDLE) {
        vkDestroyQueryPool(m_device, m_timestampQueryPool, nullptr);
        m_timestampQueryPool = VK_NULL_HANDLE;
    }
    if (m_cmd) {} // freed with pool
}

void CommandList::collectGpuTime()
{
    if (m_timestampQueryPool == VK_NULL_HANDLE || !m_timestampPending) {
        return;
    }

    // Each timestamp is followed by its availability value.
    std::array<uint64_t, 4> queryData{};
    const VkResult result = vkGetQueryPoolResults(
        m_device,
        m_timestampQueryPool,
        0,
        2,
        sizeof(queryData),
        queryData.data(),
        sizeof(uint64_t) * 2,
        VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT);

    m_timestampPending = false;
    m_statistics.gpuTimeValid = false;
    if (result == VK_NOT_READY) {
        return;
    }
    VK_CHECK(result);
    if (queryData[1] == 0 || queryData[3] == 0) {
        return;
    }

    uint64_t elapsedTicks = 0;
    if (m_timestampValidBits >= 64) {
        elapsedTicks = queryData[2] - queryData[0];
    } else {
        const uint64_t timestampMask =
            (uint64_t{1} << m_timestampValidBits) - 1;
        elapsedTicks = (queryData[2] - queryData[0]) & timestampMask;
    }

    m_statistics.gpuTimeMilliseconds =
        static_cast<double>(elapsedTicks)
        * static_cast<double>(m_timestampPeriod)
        / 1'000'000.0;
    m_statistics.gpuTimeValid = true;
}

void CommandList::begin()
{
    VK_CHECK(vkResetCommandBuffer(m_cmd, 0));
    m_statistics.drawCalls = 0;
    m_statistics.submittedVertices = 0;

    VkCommandBufferBeginInfo info{};
    info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    VK_CHECK(vkBeginCommandBuffer(m_cmd, &info));
    if (m_timestampQueryPool != VK_NULL_HANDLE) {
        vkCmdResetQueryPool(m_cmd, m_timestampQueryPool, 0, 2);
        vkCmdWriteTimestamp(
            m_cmd,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            m_timestampQueryPool,
            0);
    }
    m_recording = true;
}

void CommandList::end()
{
    if (m_timestampQueryPool != VK_NULL_HANDLE) {
        vkCmdWriteTimestamp(
            m_cmd,
            VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
            m_timestampQueryPool,
            1);
    }
    VK_CHECK(vkEndCommandBuffer(m_cmd));
    m_timestampPending = m_timestampQueryPool != VK_NULL_HANDLE;
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
    barrier.srcAccessMask = accessMaskForLayout(oldLayout, false);
    barrier.dstAccessMask = accessMaskForLayout(newLayout, true);

    vkCmdPipelineBarrier(m_cmd, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
}

void CommandList::copyBuffer(VkBuffer src, VkBuffer dst, VkDeviceSize size)
{
    VkBufferCopy region{};
    region.size = size;
    vkCmdCopyBuffer(m_cmd, src, dst, 1, &region);
}

void CommandList::copyBufferToImage(VkBuffer src, VkImage dst, uint32_t width, uint32_t height)
{
    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = {0, 0, 0};
    region.imageExtent = {width, height, 1};

    vkCmdCopyBufferToImage(m_cmd, src, dst, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
}

void CommandList::draw(
    uint32_t vertexCount,
    uint32_t instanceCount,
    uint32_t firstVertex,
    uint32_t firstInstance)
{
    vkCmdDraw(
        m_cmd,
        vertexCount,
        instanceCount,
        firstVertex,
        firstInstance);
    ++m_statistics.drawCalls;
    m_statistics.submittedVertices +=
        static_cast<uint64_t>(vertexCount) * instanceCount;
}

void CommandList::drawIndexed(
    uint32_t indexCount,
    uint32_t instanceCount,
    uint32_t firstIndex,
    int32_t vertexOffset,
    uint32_t firstInstance)
{
    vkCmdDrawIndexed(
        m_cmd,
        indexCount,
        instanceCount,
        firstIndex,
        vertexOffset,
        firstInstance);
    ++m_statistics.drawCalls;
    m_statistics.submittedVertices +=
        static_cast<uint64_t>(indexCount) * instanceCount;
}

} // namespace ku
