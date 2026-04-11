#include "SwapChain.h"
#include "RHIDevice.h"

#include <SDL3/SDL_vulkan.h>
#include <spdlog/spdlog.h>

namespace ku {

SwapChain::SwapChain(const RHIDevice& device, SDL_Window* window, VkSurfaceKHR surface)
    : m_device(&device) {
    create(window, surface);
}

SwapChain::~SwapChain() {
    destroy();
}

void SwapChain::create(SDL_Window* window, VkSurfaceKHR surface) {
    VkSurfaceCapabilitiesKHR caps;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_device->physicalDevice(), surface, &caps);

    uint32_t formatCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(m_device->physicalDevice(), surface, &formatCount, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(formatCount);
    if (formatCount > 0) {
        vkGetPhysicalDeviceSurfaceFormatsKHR(m_device->physicalDevice(), surface, &formatCount, formats.data());
    }

    uint32_t presentModeCount = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(m_device->physicalDevice(), surface, &presentModeCount, nullptr);
    std::vector<VkPresentModeKHR> presentModes(presentModeCount);
    if (presentModeCount > 0) {
        vkGetPhysicalDeviceSurfacePresentModesKHR(m_device->physicalDevice(), surface, &presentModeCount, presentModes.data());
    }

    m_imageFormat = chooseSurfaceFormat(formats).format;
    m_extent = chooseExtent(caps, window);

    uint32_t imageCount = caps.minImageCount + 1;
    if (caps.maxImageCount > 0 && imageCount > caps.maxImageCount) {
        imageCount = caps.maxImageCount;
    }

    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = surface;
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = m_imageFormat;
    createInfo.imageColorSpace = chooseSurfaceFormat(formats).colorSpace;
    createInfo.imageExtent = m_extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    if (m_device->hasSeparateQueueFamilies()) {
        uint32_t families[] = { m_device->graphicsQueueFamily(), m_device->presentQueueFamily() };
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = families;
    } else {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    createInfo.preTransform = caps.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = choosePresentMode(presentModes);
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = m_swapChain;

    VK_CHECK(vkCreateSwapchainKHR(m_device->device(), &createInfo, nullptr, &m_swapChain));

    // 获取图像
    vkGetSwapchainImagesKHR(m_device->device(), m_swapChain, &imageCount, nullptr);
    m_images.resize(imageCount);
    vkGetSwapchainImagesKHR(m_device->device(), m_swapChain, &imageCount, m_images.data());

    createImageViews();

    KU_DEBUG("SwapChain created: {} images, {}x{}", imageCount, m_extent.width, m_extent.height);
}

void SwapChain::destroy() {
    for (auto view : m_imageViews) {
        vkDestroyImageView(m_device->device(), view, nullptr);
    }
    m_imageViews.clear();
    if (m_swapChain != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(m_device->device(), m_swapChain, nullptr);
        m_swapChain = VK_NULL_HANDLE;
    }
}

void SwapChain::recreate(SDL_Window* window, VkSurfaceKHR surface) {
    m_device->waitIdle();
    destroy();
    create(window, surface);
}

uint32_t SwapChain::acquireNextImage(VkSemaphore semaphore) {
    uint32_t imageIndex = 0;
    VkResult result = vkAcquireNextImageKHR(
        m_device->device(), m_swapChain, UINT64_MAX,
        semaphore, VK_NULL_HANDLE, &imageIndex);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        // 重建后重试
        return 0;
    }

    VK_CHECK(result);
    return imageIndex;
}

void SwapChain::createImageViews() {
    m_imageViews.resize(m_images.size());
    for (size_t i = 0; i < m_images.size(); i++) {
        VkImageViewCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        createInfo.image = m_images[i];
        createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        createInfo.format = m_imageFormat;
        createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        createInfo.subresourceRange.baseMipLevel = 0;
        createInfo.subresourceRange.levelCount = 1;
        createInfo.subresourceRange.baseArrayLayer = 0;
        createInfo.subresourceRange.layerCount = 1;

        VK_CHECK(vkCreateImageView(m_device->device(), &createInfo, nullptr, &m_imageViews[i]));
    }
}

VkSurfaceFormatKHR SwapChain::chooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& available) {
    for (const auto& fmt : available) {
        if (fmt.format == VK_FORMAT_B8G8R8A8_SRGB && 
            fmt.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return fmt;
        }
    }
    return available[0];
}

VkPresentModeKHR SwapChain::choosePresentMode(const std::vector<VkPresentModeKHR>& available) {
    for (auto mode : available) {
        if (mode == VK_PRESENT_MODE_MAILBOX_KHR) return mode;
    }
    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D SwapChain::chooseExtent(const VkSurfaceCapabilitiesKHR& caps, SDL_Window* window) {
    if (caps.currentExtent.width != UINT32_MAX) {
        return caps.currentExtent;
    }
    int w = 0, h = 0;
    SDL_GetWindowSize(window, &w, &h);
    VkExtent2D extent = { static_cast<uint32_t>(w), static_cast<uint32_t>(h) };
    extent.width = std::clamp(extent.width, caps.minImageExtent.width, caps.maxImageExtent.width);
    extent.height = std::clamp(extent.height, caps.minImageExtent.height, caps.maxImageExtent.height);
    return extent;
}

} // namespace ku
