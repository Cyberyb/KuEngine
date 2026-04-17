#pragma once

#include "RHICommon.h"
#include <vulkan/vulkan.h>
#include <vma/vk_mem_alloc.h>

namespace ku {

class RHIDevice;

class RHITexture {
public:
    struct CreateInfo {
        uint32_t width = 1;
        uint32_t height = 1;
        VkFormat format = VK_FORMAT_R8G8B8A8_UNORM;
        VkImageUsageFlags usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT
                                | VK_IMAGE_USAGE_SAMPLED_BIT;
        VkImageAspectFlags aspect = VK_IMAGE_ASPECT_COLOR_BIT;
        VmaMemoryUsage memoryUsage = VMA_MEMORY_USAGE_GPU_ONLY;
    };

    RHITexture(const RHIDevice& device, uint32_t width, uint32_t height, VkFormat format);
    RHITexture(const RHIDevice& device, const CreateInfo& info);
    ~RHITexture();

    [[nodiscard]] VkImage image() const { return m_image; }
    [[nodiscard]] VkImageView imageView() const { return m_imageView; }
    [[nodiscard]] uint32_t width() const { return m_width; }
    [[nodiscard]] uint32_t height() const { return m_height; }
    [[nodiscard]] VkFormat format() const { return m_format; }
    [[nodiscard]] VkImageAspectFlags aspect() const { return m_aspect; }

private:
    const RHIDevice* m_device = nullptr;
    VkImage          m_image = VK_NULL_HANDLE;
    VkImageView      m_imageView = VK_NULL_HANDLE;
    uint32_t         m_width = 0;
    uint32_t         m_height = 0;
    VkFormat         m_format = VK_FORMAT_UNDEFINED;
    VkImageAspectFlags m_aspect = VK_IMAGE_ASPECT_COLOR_BIT;
    VmaAllocation    m_allocation = VK_NULL_HANDLE;
};

} // namespace ku
