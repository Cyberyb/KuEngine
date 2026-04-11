#include "RHIInstance.h"
#include "../Core/Log.h"

#include <GLFW/glfw3.h>

namespace ku {

RHIInstance::RHIInstance(const char* appName, uint32_t appVersion)
{
    KU_INFO("Creating Vulkan instance for: {}", appName);

    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = appName;
    appInfo.applicationVersion = appVersion;
    appInfo.apiVersion = VK_API_VERSION_1_3;

    uint32_t glfwExtCount = 0;
    const char** glfwExts = glfwGetRequiredInstanceExtensions(&glfwExtCount);
    if (!glfwExts || glfwExtCount == 0) {
        throw std::runtime_error("GLFW did not provide required Vulkan instance extensions");
    }

#if KU_DEBUG_BUILD
    m_validationEnabled = true;
#endif

    VkInstanceCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    info.pApplicationInfo = &appInfo;
    info.enabledExtensionCount = glfwExtCount;
    info.ppEnabledExtensionNames = glfwExts;
    if (m_validationEnabled) {
        info.enabledLayerCount = static_cast<uint32_t>(VALIDATION_LAYERS.size());
        info.ppEnabledLayerNames = VALIDATION_LAYERS.data();
    }

    VK_CHECK(vkCreateInstance(&info, nullptr, &m_instance));
    KU_INFO("Vulkan instance created (validation: {})", m_validationEnabled);
}

RHIInstance::~RHIInstance()
{
    if (m_instance) {
        vkDestroyInstance(m_instance, nullptr);
        KU_INFO("Vulkan instance destroyed");
    }
}

VkSurfaceKHR RHIInstance::createSurface(GLFWwindow* window) const
{
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkResult result = glfwCreateWindowSurface(m_instance, window, nullptr, &surface);
    if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to create window surface");
    }
    return surface;
}

} // namespace ku
