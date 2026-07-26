#include <set>
#include "RHIDevice.h"
#include "../Core/Log.h"

#include <algorithm>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace ku {

namespace {

struct DeviceCandidate {
    VkPhysicalDevice device = VK_NULL_HANDLE;
    VkPhysicalDeviceProperties properties{};
    VkPhysicalDeviceFeatures features{};
    VkPhysicalDeviceVulkan13Features features13{
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};
    uint32_t graphicsQueueFamily = UINT32_MAX;
    uint32_t presentQueueFamily = UINT32_MAX;
    uint64_t score = 0;
};

bool hasRequiredDeviceExtensions(VkPhysicalDevice device)
{
    uint32_t extensionCount = 0;
    VK_CHECK(vkEnumerateDeviceExtensionProperties(
        device, nullptr, &extensionCount, nullptr));

    std::vector<VkExtensionProperties> extensions(extensionCount);
    VK_CHECK(vkEnumerateDeviceExtensionProperties(
        device, nullptr, &extensionCount, extensions.data()));

    return std::any_of(
        extensions.begin(),
        extensions.end(),
        [](const VkExtensionProperties& extension) {
            return std::string_view(extension.extensionName)
                == VK_KHR_SWAPCHAIN_EXTENSION_NAME;
        });
}

bool hasUsableSwapChain(VkPhysicalDevice device, VkSurfaceKHR surface)
{
    VkSurfaceCapabilitiesKHR capabilities{};
    if (vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
            device, surface, &capabilities) != VK_SUCCESS) {
        return false;
    }
    if ((capabilities.supportedUsageFlags & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) == 0) {
        return false;
    }

    uint32_t formatCount = 0;
    if (vkGetPhysicalDeviceSurfaceFormatsKHR(
            device, surface, &formatCount, nullptr) != VK_SUCCESS
        || formatCount == 0) {
        return false;
    }

    uint32_t presentModeCount = 0;
    return vkGetPhysicalDeviceSurfacePresentModesKHR(
               device, surface, &presentModeCount, nullptr) == VK_SUCCESS
        && presentModeCount > 0;
}

std::optional<DeviceCandidate> evaluateDevice(
    VkPhysicalDevice device,
    VkSurfaceKHR surface,
    std::string& rejectionReason)
{
    DeviceCandidate candidate{};
    candidate.device = device;
    vkGetPhysicalDeviceProperties(device, &candidate.properties);

    if (candidate.properties.apiVersion < VK_API_VERSION_1_3) {
        rejectionReason = "Vulkan 1.3 is required";
        return std::nullopt;
    }
    if (!hasRequiredDeviceExtensions(device)) {
        rejectionReason = "VK_KHR_swapchain is unavailable";
        return std::nullopt;
    }

    VkPhysicalDeviceFeatures2 features2{
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
    features2.pNext = &candidate.features13;
    vkGetPhysicalDeviceFeatures2(device, &features2);
    candidate.features = features2.features;
    if (candidate.features13.dynamicRendering != VK_TRUE
        || candidate.features13.synchronization2 != VK_TRUE) {
        rejectionReason = "dynamicRendering and synchronization2 are required";
        return std::nullopt;
    }

    uint32_t queueCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueCount, nullptr);
    std::vector<VkQueueFamilyProperties> queueProperties(queueCount);
    vkGetPhysicalDeviceQueueFamilyProperties(
        device, &queueCount, queueProperties.data());

    for (uint32_t i = 0; i < queueCount; ++i) {
        VkBool32 supportsPresent = VK_FALSE;
        if (vkGetPhysicalDeviceSurfaceSupportKHR(
                device, i, surface, &supportsPresent) != VK_SUCCESS) {
            continue;
        }

        const bool supportsGraphics =
            (queueProperties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0;
        if (supportsGraphics && candidate.graphicsQueueFamily == UINT32_MAX) {
            candidate.graphicsQueueFamily = i;
        }
        if (supportsPresent && candidate.presentQueueFamily == UINT32_MAX) {
            candidate.presentQueueFamily = i;
        }
        if (supportsGraphics && supportsPresent) {
            candidate.graphicsQueueFamily = i;
            candidate.presentQueueFamily = i;
            break;
        }
    }

    if (candidate.graphicsQueueFamily == UINT32_MAX) {
        rejectionReason = "no graphics queue family";
        return std::nullopt;
    }
    if (candidate.presentQueueFamily == UINT32_MAX) {
        rejectionReason = "no presentation queue family";
        return std::nullopt;
    }
    if (!hasUsableSwapChain(device, surface)) {
        rejectionReason = "surface has no usable SwapChain format or present mode";
        return std::nullopt;
    }

    switch (candidate.properties.deviceType) {
        case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
            candidate.score += 10'000;
            break;
        case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
            candidate.score += 5'000;
            break;
        case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
            candidate.score += 2'500;
            break;
        default:
            candidate.score += 1'000;
            break;
    }
    candidate.score += candidate.properties.limits.maxImageDimension2D;
    return candidate;
}

} // namespace

RHIDevice::RHIDevice(VkInstance instance, VkSurfaceKHR surface)
    : m_instance(instance)
{
    pickPhysicalDevice(surface);
    try {
        createLogicalDevice();
        initVMA();
    } catch (...) {
        if (m_allocator != VK_NULL_HANDLE) {
            vmaDestroyAllocator(m_allocator);
            m_allocator = VK_NULL_HANDLE;
        }
        if (m_device != VK_NULL_HANDLE) {
            vkDestroyDevice(m_device, nullptr);
            m_device = VK_NULL_HANDLE;
        }
        throw;
    }
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
    VK_CHECK(vkEnumeratePhysicalDevices(m_instance, &count, nullptr));
    if (count == 0) throw std::runtime_error("No Vulkan-capable GPU found");

    std::vector<VkPhysicalDevice> devices(count);
    VK_CHECK(vkEnumeratePhysicalDevices(m_instance, &count, devices.data()));

    std::optional<DeviceCandidate> selected;
    for (const VkPhysicalDevice device : devices) {
        VkPhysicalDeviceProperties properties{};
        vkGetPhysicalDeviceProperties(device, &properties);

        std::string rejectionReason;
        std::optional<DeviceCandidate> candidate =
            evaluateDevice(device, surface, rejectionReason);
        if (!candidate) {
            KU_WARN(
                "Skipping GPU '{}': {}",
                properties.deviceName,
                rejectionReason);
            continue;
        }

        KU_INFO(
            "Compatible GPU candidate: {} (score={})",
            candidate->properties.deviceName,
            candidate->score);
        if (!selected || candidate->score > selected->score) {
            selected = std::move(candidate);
        }
    }

    if (!selected) {
        throw std::runtime_error(
            "No GPU satisfies the Runtime Vulkan 1.3, queue, feature, and SwapChain requirements");
    }

    m_physicalDevice = selected->device;
    m_properties = selected->properties;
    m_features = selected->features;
    m_features13 = selected->features13;
    m_graphicsQueueFamily = selected->graphicsQueueFamily;
    m_presentQueueFamily = selected->presentQueueFamily;
    vkGetPhysicalDeviceMemoryProperties(m_physicalDevice, &m_memoryProperties);

    KU_INFO(
        "Selected GPU: {} (graphics queue={}, present queue={})",
        m_properties.deviceName,
        m_graphicsQueueFamily,
        m_presentQueueFamily);
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

    VkPhysicalDeviceVulkan13Features enabledFeatures13{
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};
    enabledFeatures13.dynamicRendering = VK_TRUE;
    enabledFeatures13.synchronization2 = VK_TRUE;
    info.pNext = &enabledFeatures13;

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
