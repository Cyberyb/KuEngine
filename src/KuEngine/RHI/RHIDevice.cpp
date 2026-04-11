#include "RHIDevice.h"
#include "RHIInstance.h"

#include <spdlog/spdlog.h>

namespace ku {

RHIDevice::RHIDevice(const RHIInstance& instance, VkSurfaceKHR surface)
    : m_instance(&instance), m_instanceHandle(instance.instance()) {
    pickPhysicalDevice(surface);
    createLogicalDevice();
    initVMA();
}

RHIDevice::~RHIDevice() {
    vmaDestroyAllocator(m_allocator);
    vkDestroyDevice(m_device, nullptr);
}

void RHIDevice::pickPhysicalDevice(VkSurfaceKHR surface) {
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(m_instanceHandle, &deviceCount, nullptr);
    if (deviceCount == 0) {
        throw std::runtime_error("No Vulkan-capable GPU found");
    }

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(m_instanceHandle, &deviceCount, devices.data());

    for (const auto& device : devices) {
        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(device, &props);
        vkGetPhysicalDeviceFeatures(device, &m_features);

        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

        int graphicsFamily = -1;
        int presentFamily = -1;

        for (uint32_t i = 0; i < queueFamilyCount; i++) {
            if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                graphicsFamily = i;
            }
            VkBool32 support = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &support);
            if (support) {
                presentFamily = i;
            }
        }

        if (graphicsFamily >= 0 && presentFamily >= 0) {
            m_physicalDevice = device;
            m_graphicsQueueFamily = static_cast<uint32_t>(graphicsFamily);
            m_presentQueueFamily = static_cast<uint32_t>(presentFamily);
            m_properties = props;
            break;
        }
    }

    if (m_physicalDevice == VK_NULL_HANDLE) {
        throw std::runtime_error("No suitable GPU found");
    }

    vkGetPhysicalDeviceMemoryProperties(m_physicalDevice, &m_memoryProperties);

    KU_INFO("Selected GPU: {}", m_properties.deviceName);
    KU_INFO("  Device Type: {}",
        m_properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU ? "Discrete" :
        m_properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU ? "Integrated" : "Other");
    KU_INFO("  Vulkan API: {}.{}.{}",
        VK_API_VERSION_MAJOR(m_properties.apiVersion),
        VK_API_VERSION_MINOR(m_properties.apiVersion),
        VK_API_VERSION_PATCH(m_properties.apiVersion));
}

void RHIDevice::createLogicalDevice() {
    std::vector<VkDeviceQueueCreateInfo> queueCreates;

    std::set<uint32_t> uniqueFamilies = { m_graphicsQueueFamily };
    if (m_graphicsQueueFamily != m_presentQueueFamily) {
        uniqueFamilies.insert(m_presentQueueFamily);
    }

    float queuePriority = 1.0f;
    for (uint32_t family : uniqueFamilies) {
        VkDeviceQueueCreateInfo qi{};
        qi.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        qi.queueFamilyIndex = family;
        qi.queueCount = 1;
        qi.pQueuePriorities = &queuePriority;
        queueCreates.push_back(qi);
    }

    const char* extensions[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

    VkPhysicalDeviceFeatures enabledFeatures{};
    enabledFeatures.samplerAnisotropy = m_features.samplerAnisotropy;

    // 检查 dynamicRendering 支持
    VkPhysicalDeviceDynamicRenderingFeatures dynamicRendering{};
    dynamicRendering.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES;
    dynamicRendering.dynamicRendering = VK_TRUE;

    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.pNext = &dynamicRendering;
    createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreates.size());
    createInfo.pQueueCreateInfos = queueCreates.data();
    createInfo.pEnabledFeatures = &enabledFeatures;
    createInfo.enabledExtensionCount = 1;
    createInfo.ppEnabledExtensionNames = extensions;

#ifdef KU_DEBUG_BUILD
    const char* layers[] = { "VK_LAYER_KHRONOS_validation" };
    createInfo.enabledLayerCount = 1;
    createInfo.ppEnabledLayerNames = layers;
#endif

    VK_CHECK(vkCreateDevice(m_physicalDevice, &createInfo, nullptr, &m_device));

    vkGetDeviceQueue(m_device, m_graphicsQueueFamily, 0, &m_graphicsQueue);
    vkGetDeviceQueue(m_device, m_presentQueueFamily, 0, &m_presentQueue);
}

void RHIDevice::initVMA() {
    VmaAllocatorCreateInfo createInfo{};
    createInfo.physicalDevice = m_physicalDevice;
    createInfo.device = m_device;
    createInfo.instance = m_instanceHandle;
    createInfo.vulkanApiVersion = VK_API_VERSION_1_3;

    VK_CHECK(vmaCreateAllocator(&createInfo, &m_allocator));
}

uint32_t RHIDevice::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags flags) const {
    for (uint32_t i = 0; i < m_memoryProperties.memoryTypeCount; i++) {
        if (typeFilter & (1 << i)) {
            if ((m_memoryProperties.memoryTypes[i].propertyFlags & flags) == flags) {
                return i;
            }
        }
    }
    throw std::runtime_error("Failed to find suitable memory type");
}

void RHIDevice::waitIdle() const {
    vkDeviceWaitIdle(m_device);
}

VkCommandPool RHIDevice::createCommandPool() const {
    VkCommandPoolCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    createInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    createInfo.queueFamilyIndex = m_graphicsQueueFamily;

    VkCommandPool pool = VK_NULL_HANDLE;
    VK_CHECK(vkCreateCommandPool(m_device, &createInfo, nullptr, &pool));
    return pool;
}

} // namespace ku
