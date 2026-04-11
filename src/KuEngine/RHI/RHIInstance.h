#pragma once

#include "RHICommon.h"
#include <vulkan/vulkan.h>
#include <vector>
#include <string>

namespace ku {

class RHIInstance {
public:
    RHIInstance(const char* appName, uint32_t appVersion);
    ~RHIInstance();

    [[nodiscard]] VkInstance instance() const { return m_instance; }
    [[nodiscard]] bool validationEnabled() const { return m_validationEnabled; }

    // 创建 Surface（从 SDL_Window）
    [[nodiscard]] VkSurfaceKHR createSurface(SDL_Window* window) const;

    static const std::vector<const char*>& requiredExtensions();

private:
    void create();
    void destroy();
    void setupDebugMessenger();

    VkInstance               m_instance = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT  m_debugMessenger = VK_NULL_HANDLE;
    bool                     m_validationEnabled = false;

    static constexpr const char* APP_NAME = "KuEngine";
    static constexpr uint32_t ENGINE_VERSION = VK_MAKE_VERSION(0, 1, 0);

    // Validation layers
    static constexpr std::array VALIDATION_LAYERS = {
        "VK_LAYER_KHRONOS_validation"
    };

    // Debug callback
    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT,
        VkDebugUtilsMessageTypeFlagsEXT,
        const VkDebugUtilsMessengerCallbackDataEXT*,
        void*);
};

} // namespace ku
