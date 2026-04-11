#include "RHITexture.h"
#include "RHIDevice.h"

namespace ku {

RHITexture::RHITexture(const RHIDevice& device, uint32_t width, uint32_t height,
    VkFormat format, VkImageUsageFlags usage, VkImageAspectFlags aspect)
    : m_format(format), m_width(width), m_height(height), m_device(&device) {
    
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent = { width, height, 1 };
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = format;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = usage;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    VK_CHECK(vmaCreateImage(device.allocator(), &imageInfo, &allocInfo, 
        &m_image, &m_allocation, nullptr));

    m_imageView = createImageView(m_image, format, aspect);
}

RHITexture::~RHITexture() {
    if (m_imageView != VK_NULL_HANDLE) {
        vkDestroyImageView(m_device->device(), m_imageView, nullptr);
    }
    if (m_image != VK_NULL_HANDLE && m_device) {
        vmaDestroyImage(m_device->allocator(), m_image, m_allocation);
    }
}

RHITexture::RHITexture(RHITexture&& other) noexcept
    : m_image(other.m_image), m_imageView(other.m_imageView),
      m_allocation(other.m_allocation), m_format(other.m_format),
      m_width(other.m_width), m_height(other.m_height),
      m_device(other.m_device) {
    other.m_image = VK_NULL_HANDLE;
    other.m_imageView = VK_NULL_HANDLE;
    other.m_allocation = VK_NULL_HANDLE;
}

RHITexture& RHITexture::operator=(RHITexture&& other) noexcept {
    if (this != &other) {
        if (m_imageView != VK_NULL_HANDLE) vkDestroyImageView(m_device->device(), m_imageView, nullptr);
        if (m_image != VK_NULL_HANDLE) vmaDestroyImage(m_device->allocator(), m_image, m_allocation);
        m_image = other.m_image;
        m_imageView = other.m_imageView;
        m_allocation = other.m_allocation;
        m_format = other.m_format;
        m_width = other.m_width;
        m_height = other.m_height;
        m_device = other.m_device;
        other.m_image = VK_NULL_HANDLE;
        other.m_imageView = VK_NULL_HANDLE;
        other.m_allocation = VK_NULL_HANDLE;
    }
    return *this;
}

VkImageView RHITexture::createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspect) const {
    VkImageViewCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    createInfo.image = image;
    createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    createInfo.format = format;
    createInfo.subresourceRange.aspectMask = aspect;
    createInfo.subresourceRange.baseMipLevel = 0;
    createInfo.subresourceRange.levelCount = 1;
    createInfo.subresourceRange.baseArrayLayer = 0;
    createInfo.subresourceRange.layerCount = 1;

    VkImageView view = VK_NULL_HANDLE;
    VK_CHECK(vkCreateImageView(m_device->device(), &createInfo, nullptr, &view));
    return view;
}

} // namespace ku
