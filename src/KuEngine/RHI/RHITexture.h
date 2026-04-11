#pragma once

#include "RHICommon.h"
#include <vulkan/vulkan.h>
#include <vma/vk_mem_alloc.h>

namespace ku {

class RHIDevice;

class RHITexture {
public:
    RHITexture(const RHIDevice& device, uint32_t width, uint32_t height, VkFormat format);
    ~RHITexture();

    [[nodiscard]] VkImage image() const { return m_image; }
    [[nodiscard]] VkImageView imageView() const { return m_imageView; }
    [[nodiscard]] uint32_t width() const { return m_width; }
    [[nodiscard]] uint32_t height() const { return m_height; }
    [[nodiscard]] VkFormat format() const { return m_format; }

private:
    const RHIDevice* m_device = nullptr;
    VkImage          m_image = VK_NULL_HANDLE;
    VkImageView      m_imageView = VK_NULL_HANDLE;
    uint32_t         m_width = 0;
    uint32_t         m_height = 0;
    VkFormat         m_format = VK_FORMAT_UNDEFINED;
    VmaAllocation    m_allocation = VK_NULL_HANDLE;
};

} // namespace ku
