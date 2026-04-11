#pragma once

#include "RHICommon.h"
#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

namespace ku {

class RHIDevice {
public:
    RHIDevice(VkInstance instance, VkSurfaceKHR surface);
    ~RHIDevice();

    [[nodiscard]] VkInstance instance() const { return m_instance; }
    [[nodiscard]] VkPhysicalDevice physicalDevice() const { return m_physicalDevice; }
    [[nodiscard]] VkDevice device() const { return m_device; }
    [[nodiscard]] VmaAllocator allocator() const { return m_allocator; }
    [[nodiscard]] VkQueue graphicsQueue() const { return m_graphicsQueue; }
    [[nodiscard]] VkQueue presentQueue() const { return m_presentQueue; }
    [[nodiscard]] uint32_t graphicsQueueFamily() const { return m_graphicsQueueFamily; }
    [[nodiscard]] uint32_t presentQueueFamily() const { return m_presentQueueFamily; }
    [[nodiscard]] const VkPhysicalDeviceProperties& properties() const { return m_properties; }
    [[nodiscard]] const VkPhysicalDeviceFeatures& features() const { return m_features; }
    [[nodiscard]] bool hasSeparateQueueFamilies() const { 
        return m_graphicsQueueFamily != m_presentQueueFamily; 
    }

    [[nodiscard]] uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags flags) const;
    void waitIdle() const;
    [[nodiscard]] VkCommandPool createCommandPool() const;

private:
    void pickPhysicalDevice(VkSurfaceKHR surface);
    void createLogicalDevice();
    void initVMA();

    VkInstance m_instance = VK_NULL_HANDLE;
    VkPhysicalDevice                  m_physicalDevice = VK_NULL_HANDLE;
    VkDevice                         m_device = VK_NULL_HANDLE;
    VmaAllocator                     m_allocator = VK_NULL_HANDLE;
    VkQueue                          m_graphicsQueue = VK_NULL_HANDLE;
    VkQueue                          m_presentQueue = VK_NULL_HANDLE;
    uint32_t                         m_graphicsQueueFamily = UINT32_MAX;
    uint32_t                         m_presentQueueFamily = UINT32_MAX;

    VkPhysicalDeviceProperties       m_properties{};
    VkPhysicalDeviceFeatures        m_features{};
    VkPhysicalDeviceMemoryProperties m_memoryProperties{};
};

} // namespace ku
