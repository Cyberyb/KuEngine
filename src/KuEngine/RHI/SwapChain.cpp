#include <algorithm>
#include "SwapChain.h"
#include "RHIDevice.h"
#include "../Core/Log.h"

namespace ku {

SwapChain::SwapChain(const RHIDevice& device, GLFWwindow* window, VkSurfaceKHR surface)
    : m_device(&device)
{
    create(window, surface);
}

SwapChain::~SwapChain()
{
    destroy();
}

void SwapChain::create(GLFWwindow* window, VkSurfaceKHR surface)
{
    const RHIDevice& device = *m_device;

    VkSurfaceCapabilitiesKHR caps;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device.physicalDevice(), surface, &caps);

    std::vector<VkSurfaceFormatKHR> formats;
    uint32_t formatCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device.physicalDevice(), surface, &formatCount, nullptr);
    formats.resize(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(device.physicalDevice(), surface, &formatCount, formats.data());

    std::vector<VkPresentModeKHR> modes;
    uint32_t modeCount = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device.physicalDevice(), surface, &modeCount, nullptr);
    modes.resize(modeCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(device.physicalDevice(), surface, &modeCount, modes.data());

    VkSurfaceFormatKHR surfaceFormat = chooseSurfaceFormat(formats);
    VkPresentModeKHR presentMode = choosePresentMode(modes);
    VkExtent2D extent = chooseExtent(caps, window);

    uint32_t imageCount = caps.minImageCount + 1;
    if (caps.maxImageCount > 0) imageCount = std::min(imageCount, caps.maxImageCount);

    VkSwapchainCreateInfoKHR info{};
    info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    info.surface = surface;
    info.minImageCount = imageCount;
    info.imageFormat = surfaceFormat.format;
    info.imageColorSpace = surfaceFormat.colorSpace;
    info.imageExtent = extent;
    info.imageArrayLayers = 1;
    info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    info.preTransform = caps.currentTransform;
    info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    info.presentMode = presentMode;
    info.clipped = VK_TRUE;
    info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (device.hasSeparateQueueFamilies()) {
        uint32_t indices[] = {device.graphicsQueueFamily(), device.presentQueueFamily()};
        info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        info.queueFamilyIndexCount = 2;
        info.pQueueFamilyIndices = indices;
    }

    VK_CHECK(vkCreateSwapchainKHR(device.device(), &info, nullptr, &m_swapChain));

    vkGetSwapchainImagesKHR(device.device(), m_swapChain, &imageCount, nullptr);
    m_images.resize(imageCount);
    vkGetSwapchainImagesKHR(device.device(), m_swapChain, &imageCount, m_images.data());

    m_imageFormat = surfaceFormat.format;
    m_extent = extent;

    createImageViews();
    KU_INFO("SwapChain created: {}x{}", m_extent.width, m_extent.height);
}

void SwapChain::destroy()
{
    auto dev = m_device->device();
    for (auto view : m_imageViews) vkDestroyImageView(dev, view, nullptr);
    m_imageViews.clear();
    if (m_swapChain) {
        vkDestroySwapchainKHR(dev, m_swapChain, nullptr);
        m_swapChain = VK_NULL_HANDLE;
    }
}

void SwapChain::createImageViews()
{
    auto dev = m_device->device();
    m_imageViews.resize(m_images.size());
    for (size_t i = 0; i < m_images.size(); ++i) {
        VkImageViewCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        info.image = m_images[i];
        info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        info.format = m_imageFormat;
        info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        info.subresourceRange.baseMipLevel = 0;
        info.subresourceRange.levelCount = 1;
        info.subresourceRange.baseArrayLayer = 0;
        info.subresourceRange.layerCount = 1;
        VK_CHECK(vkCreateImageView(dev, &info, nullptr, &m_imageViews[i]));
    }
}

uint32_t SwapChain::acquireNextImage(VkSemaphore semaphore)
{
    uint32_t imageIndex = 0;
    vkAcquireNextImageKHR(m_device->device(), m_swapChain, UINT64_MAX,
                          semaphore, VK_NULL_HANDLE, &imageIndex);
    return imageIndex;
}

void SwapChain::recreate(GLFWwindow* window, VkSurfaceKHR surface)
{
    m_device->waitIdle();
    destroy();
    create(window, surface);
}

VkSurfaceFormatKHR SwapChain::chooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& available)
{
    for (const auto& fmt : available) {
        if (fmt.format == VK_FORMAT_B8G8R8A8_UNORM &&
            fmt.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
            return fmt;
    }
    return available[0];
}

VkPresentModeKHR SwapChain::choosePresentMode(const std::vector<VkPresentModeKHR>& available)
{
    for (const auto& mode : available) {
        if (mode == VK_PRESENT_MODE_MAILBOX_KHR) return mode;
    }
    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D SwapChain::chooseExtent(const VkSurfaceCapabilitiesKHR& caps, GLFWwindow* window)
{
    if (caps.currentExtent.width != UINT32_MAX) return caps.currentExtent;
    int w = 0, h = 0;
    glfwGetFramebufferSize(window, &w, &h);
    VkExtent2D result = {static_cast<uint32_t>(w), static_cast<uint32_t>(h)};
    result.width = std::clamp(result.width, caps.minImageExtent.width, caps.maxImageExtent.width);
    result.height = std::clamp(result.height, caps.minImageExtent.height, caps.maxImageExtent.height);
    return result;
}

} // namespace ku
