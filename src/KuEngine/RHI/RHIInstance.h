// KuEngine RHI 实例模块：管理 Vulkan 实例、调试验证与窗口表面的创建和销毁。
#pragma once

#include "RHICommon.h"
#include "VulkanValidation.h"
#include <vulkan/vulkan.h>
#include <array>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

namespace ku {

class RHIInstance {
public:
    RHIInstance(
        const char* appName,
        uint32_t appVersion,
        std::shared_ptr<ValidationMessageTracker> validationMessages = {});
    ~RHIInstance();

    [[nodiscard]] VkInstance instance() const { return m_instance; }
    [[nodiscard]] bool validationRequested() const noexcept {
        return m_validationState.requested();
    }
    [[nodiscard]] bool validationEnabled() const noexcept {
        return m_validationState.validationEnabled();
    }
    [[nodiscard]] bool validationMessageCaptureEnabled() const {
        return m_validationState.messageCaptureEnabled();
    }
    [[nodiscard]] ValidationMessageCounts validationMessageCounts() const noexcept {
        return m_validationState.counts();
    }
    void resetValidationMessageCounts() noexcept { m_validationState.reset(); }
    [[nodiscard]] bool submitValidationTestErrorForSmokeTest() const;

    [[nodiscard]] VkSurfaceKHR createSurface(GLFWwindow* window) const;

private:
    VkInstance               m_instance = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT m_debugMessenger = VK_NULL_HANDLE;
    VulkanValidationState    m_validationState;

    static constexpr std::array<const char*, 1> VALIDATION_LAYERS = {
        "VK_LAYER_KHRONOS_validation"
    };
};

} // namespace ku
