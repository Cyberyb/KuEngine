#pragma once

#include "RHICommon.h"
#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

namespace ku {

class RHIDevice;

class RHITexture {
public:
    RHITexture() = default;
    RHITexture(const RHIDevice& device, uint32_t width, uint32_t height,
               VkFormat format, VkImageUsageFlags usage, VkImageAspectFlags aspect);
    ~RHITexture();

    // 禁止拷贝
    RHITexture(const RHITexture&) = delete;
    RHITexture& operator=(const RHITexture&) = delete;

    // 移动
    RHITexture(RHITexture&& other) noexcept;
    RHITexture& operator=(RHITexture&& other) noexcept;

    [[nodiscard]] VkImage image() const { return m_image; }
    [[nodiscard]] VkImageView imageView() const { return m_imageView; }
    [[nodiscard]] VkFormat format() const { return m_format; }
    [[nodiscard]] uint32_t width() const { return m_width; }
    [[nodiscard]] uint32_t height() const { return m_height; }

private:
    VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspect) const;

    VkImage       m_image = VK_NULL_HANDLE;
    VkImageView   m_imageView = VK_NULL_HANDLE;
    VmaAllocation m_allocation = VK_NULL_HANDLE;
    VkFormat      m_format = VK_FORMAT_UNDEFINED;
    uint32_t      m_width = 0;
    uint32_t      m_height = 0;

    const RHIDevice* m_device = nullptr;
};

} // namespace ku
