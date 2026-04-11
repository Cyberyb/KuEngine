#pragma once

#include "RHICommon.h"
#include <vulkan/vulkan.h>
#include <array>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

namespace ku {

class RHIInstance {
public:
    RHIInstance(const char* appName, uint32_t appVersion);
    ~RHIInstance();

    [[nodiscard]] VkInstance instance() const { return m_instance; }
    [[nodiscard]] bool validationEnabled() const { return m_validationEnabled; }

    [[nodiscard]] VkSurfaceKHR createSurface(GLFWwindow* window) const;

private:
    VkInstance               m_instance = VK_NULL_HANDLE;
    bool                     m_validationEnabled = false;

    static constexpr std::array<const char*, 1> VALIDATION_LAYERS = {
        "VK_LAYER_KHRONOS_validation"
    };
};

} // namespace ku
