#pragma once

#include "RHICommon.h"
#include <vulkan/vulkan.h>
#include <vector>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

namespace ku {

class RHIDevice;

class SwapChain {
public:
    SwapChain(const RHIDevice& device, GLFWwindow* window, VkSurfaceKHR surface);
    ~SwapChain();

    void recreate(GLFWwindow* window, VkSurfaceKHR surface);
    [[nodiscard]] uint32_t acquireNextImage(VkSemaphore semaphore);

    [[nodiscard]] VkSwapchainKHR swapChain() const { return m_swapChain; }
    [[nodiscard]] VkFormat imageFormat() const { return m_imageFormat; }
    [[nodiscard]] const std::vector<VkImage>& images() const { return m_images; }
    [[nodiscard]] VkExtent2D extent() const { return m_extent; }
    [[nodiscard]] const std::vector<VkImageView>& imageViews() const { return m_imageViews; }
    [[nodiscard]] size_t imageCount() const { return m_images.size(); }
    [[nodiscard]] uint32_t width() const { return m_extent.width; }
    [[nodiscard]] uint32_t height() const { return m_extent.height; }
    [[nodiscard]] float aspectRatio() const {
        return static_cast<float>(m_extent.width) / static_cast<float>(m_extent.height);
    }

private:
    void create(GLFWwindow* window, VkSurfaceKHR surface);
    void destroy();
    void createImageViews();
    VkSurfaceFormatKHR chooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& available);
    VkPresentModeKHR choosePresentMode(const std::vector<VkPresentModeKHR>& available);
    VkExtent2D chooseExtent(const VkSurfaceCapabilitiesKHR& caps, GLFWwindow* window);

    const RHIDevice* m_device = nullptr;
    VkSwapchainKHR   m_swapChain = VK_NULL_HANDLE;
    std::vector<VkImage> m_images;
    std::vector<VkImageView> m_imageViews;
    VkFormat         m_imageFormat = VK_FORMAT_UNDEFINED;
    VkExtent2D      m_extent = {0, 0};
    bool m_needsRebuild = false;
};

} // namespace ku
