#include <set>
#include "RHIDevice.h"
#include "../Core/Log.h"

#include <algorithm>

namespace ku {

RHIDevice::RHIDevice(VkInstance instance, VkSurfaceKHR surface)
    : m_instance(instance)
{
    pickPhysicalDevice(surface);
    createLogicalDevice();
    initVMA();
    KU_INFO("RHI device created");
}

RHIDevice::~RHIDevice()
{
    if (m_allocator != VK_NULL_HANDLE) vmaDestroyAllocator(m_allocator);
    if (m_device != VK_NULL_HANDLE) {
        vkDestroyDevice(m_device, nullptr);
        KU_INFO("RHI device destroyed");
    }
}

void RHIDevice::pickPhysicalDevice(VkSurfaceKHR surface)
{
    uint32_t count = 0;
    vkEnumeratePhysicalDevices(m_instance, &count, nullptr);
    if (count == 0) throw std::runtime_error("No Vulkan-capable GPU found");

    std::vector<VkPhysicalDevice> devices(count);
    vkEnumeratePhysicalDevices(m_instance, &count, devices.data());

    for (const auto& dev : devices) {
        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(dev, &props);
        if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
            m_physicalDevice = dev;
            break;
        }
    }
    if (m_physicalDevice == VK_NULL_HANDLE) m_physicalDevice = devices[0];

    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(m_physicalDevice, &props);
    m_properties = props;
    vkGetPhysicalDeviceFeatures(m_physicalDevice, &m_features);
    vkGetPhysicalDeviceMemoryProperties(m_physicalDevice, &m_memoryProperties);

    KU_INFO("GPU: {}", props.deviceName);

    uint32_t qCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(m_physicalDevice, &qCount, nullptr);
    std::vector<VkQueueFamilyProperties> qProps(qCount);
    vkGetPhysicalDeviceQueueFamilyProperties(m_physicalDevice, &qCount, qProps.data());

    bool foundGraphics = false;
    for (uint32_t i = 0; i < qCount; ++i) {
        if (qProps[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            m_graphicsQueueFamily = i;
            foundGraphics = true;
            break;
        }
    }
    if (!foundGraphics) throw std::runtime_error("No graphics queue family");

    for (uint32_t i = 0; i < qCount; ++i) {
        VkBool32 support = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(m_physicalDevice, i, surface, &support);
        if (support) { m_presentQueueFamily = i; break; }
    }

    if (m_presentQueueFamily == UINT32_MAX) {
        m_presentQueueFamily = m_graphicsQueueFamily;
    }
}

void RHIDevice::createLogicalDevice()
{
    std::vector<VkDeviceQueueCreateInfo> qInfos;
    std::set<uint32_t> uniqueFamilies = {m_graphicsQueueFamily, m_presentQueueFamily};
    float priority = 1.0f;
    for (uint32_t f : uniqueFamilies) {
        VkDeviceQueueCreateInfo qi{};
        qi.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        qi.queueFamilyIndex = f;
        qi.queueCount = 1;
        qi.pQueuePriorities = &priority;
        qInfos.push_back(qi);
    }

    const char* extensions[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

    VkDeviceCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    info.queueCreateInfoCount = static_cast<uint32_t>(qInfos.size());
    info.pQueueCreateInfos = qInfos.data();
    info.pEnabledFeatures = &m_features;
    info.enabledExtensionCount = 1;
    info.ppEnabledExtensionNames = extensions;

    VK_CHECK(vkCreateDevice(m_physicalDevice, &info, nullptr, &m_device));
    vkGetDeviceQueue(m_device, m_graphicsQueueFamily, 0, &m_graphicsQueue);
    vkGetDeviceQueue(m_device, m_presentQueueFamily, 0, &m_presentQueue);
}

void RHIDevice::initVMA()
{
    VmaAllocatorCreateInfo info{};
    info.physicalDevice = m_physicalDevice;
    info.device = m_device;
    info.instance = m_instance;
    VK_CHECK(vmaCreateAllocator(&info, &m_allocator));
}

uint32_t RHIDevice::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags flags) const
{
    for (uint32_t i = 0; i < m_memoryProperties.memoryTypeCount; ++i) {
        if (typeFilter & (1 << i)) {
            if ((m_memoryProperties.memoryTypes[i].propertyFlags & flags) == flags)
                return i;
        }
    }
    throw std::runtime_error("Failed to find suitable memory type");
}

void RHIDevice::waitIdle() const { vkDeviceWaitIdle(m_device); }

VkCommandPool RHIDevice::createCommandPool() const
{
    VkCommandPoolCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    info.queueFamilyIndex = m_graphicsQueueFamily;
    info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    VkCommandPool pool = VK_NULL_HANDLE;
    VK_CHECK(vkCreateCommandPool(m_device, &info, nullptr, &pool));
    return pool;
}

} // namespace ku
