#include "RHITexture.h"
#include "RHIDevice.h"

namespace ku {

RHITexture::RHITexture(const RHIDevice& device, uint32_t width, uint32_t height, VkFormat format)
    : RHITexture(device, CreateInfo{width, height, format})
{}

RHITexture::RHITexture(const RHIDevice& device, const CreateInfo& info)
    : m_device(&device)
    , m_width(info.width)
    , m_height(info.height)
    , m_format(info.format)
    , m_aspect(info.aspect)
{
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent = {m_width, m_height, 1};
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = m_format;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage = info.usage;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = info.memoryUsage;
    VK_CHECK(vmaCreateImage(
        m_device->allocator(),
        &imageInfo,
        &allocInfo,
        &m_image,
        &m_allocation,
        nullptr));

    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = m_image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = m_format;
    viewInfo.subresourceRange.aspectMask = m_aspect;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;
    VK_CHECK(vkCreateImageView(m_device->device(), &viewInfo, nullptr, &m_imageView));
}

RHITexture::~RHITexture()
{
    if (!m_device) {
        return;
    }

    if (m_imageView) {
        vkDestroyImageView(m_device->device(), m_imageView, nullptr);
    }

    if (m_image != VK_NULL_HANDLE && m_allocation != VK_NULL_HANDLE) {
        vmaDestroyImage(m_device->allocator(), m_image, m_allocation);
    }
}

} // namespace ku
