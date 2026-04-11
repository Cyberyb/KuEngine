#include "RHIInstance.h"

#include <SDL3/SDL_vulkan.h>
#include <spdlog/spdlog.h>
#include <vector>

namespace ku {

// ============== RHIInstance ==============

RHIInstance::RHIInstance(const char* appName, uint32_t appVersion) {
    // 检查 Validation Layer 支持
#ifdef KU_DEBUG_BUILD
    m_validationEnabled = true;
#else
    m_validationEnabled = false;
#endif

    // 填充 VkApplicationInfo
    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = appName;
    appInfo.applicationVersion = appVersion;
    appInfo.pEngineName = "KuEngine";
    appInfo.engineVersion = VK_MAKE_VERSION(KU_VERSION_MAJOR, KU_VERSION_MINOR, KU_VERSION_PATCH);
    appInfo.apiVersion = VK_API_VERSION_1_3;

    // 获取 SDL 要求的扩展
    uint32_t sdlExtensionCount = 0;
    if (!SDL_Vulkan_GetInstanceExtensions(&sdlExtensionCount, nullptr)) {
        throw std::runtime_error("Failed to get SDL Vulkan extensions");
    }
    std::vector<const char*> extensions(sdlExtensionCount);
    SDL_Vulkan_GetInstanceExtensions(&sdlExtensionCount, extensions.data());

    // 添加 Debug Utils 扩展
    if (m_validationEnabled) {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    // 创建 Instance
    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();

    // Validation Layers
    if (m_validationEnabled) {
        createInfo.enabledLayerCount = static_cast<uint32_t>(VALIDATION_LAYERS.size());
        createInfo.ppEnabledLayerNames = VALIDATION_LAYERS.data();
    }

    VK_CHECK(vkCreateInstance(&createInfo, nullptr, &m_instance));

    if (m_validationEnabled) {
        setupDebugMessenger();
    }

    KU_INFO("Vulkan Instance created (API {}.{}.{})",
        VK_API_VERSION_MAJOR(appInfo.apiVersion),
        VK_API_VERSION_MINOR(appInfo.apiVersion),
        VK_API_VERSION_PATCH(appInfo.apiVersion));
}

RHIInstance::~RHIInstance() {
    if (m_validationEnabled && m_debugMessenger != VK_NULL_HANDLE) {
        auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)
            vkGetInstanceProcAddr(m_instance, "vkDestroyDebugUtilsMessengerEXT");
        if (func) {
            func(m_instance, m_debugMessenger, nullptr);
        }
    }
    vkDestroyInstance(m_instance, nullptr);
}

VkSurfaceKHR RHIInstance::createSurface(SDL_Window* window) const {
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    if (!SDL_Vulkan_CreateSurface(window, m_instance, nullptr, &surface)) {
        throw std::runtime_error("Failed to create Vulkan surface");
    }
    return surface;
}

const std::vector<const char*>& RHIInstance::requiredExtensions() {
    static std::vector<const char*> extensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    };
    return extensions;
}

void RHIInstance::setupDebugMessenger() {
    VkDebugUtilsMessengerCreateInfoEXT createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    createInfo.messageSeverity =
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    createInfo.messageType =
        VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    createInfo.pfnUserCallback = debugCallback;
    createInfo.pUserData = nullptr;

    auto func = (PFN_vkCreateDebugUtilsMessengerEXT)
        vkGetInstanceProcAddr(m_instance, "vkCreateDebugUtilsMessengerEXT");
    if (func) {
        func(m_instance, &createInfo, nullptr, &m_debugMessenger);
    }
}

VKAPI_ATTR VkBool32 VKAPI_CALL
RHIInstance::debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT,
    const VkDebugUtilsMessengerCallbackDataEXT* data,
    void*) {
    
    if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
        std::cerr << "[VULKAN ERROR] " << data->pMessage << "\n";
    } else if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        std::cerr << "[VULKAN WARN] " << data->pMessage << "\n";
    }
    return VK_FALSE;
}

} // namespace ku
