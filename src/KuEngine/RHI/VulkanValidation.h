// KuEngine Vulkan 验证诊断：提供线程安全的 Validation 消息计数。
#pragma once

#include <vulkan/vulkan.h>

#include <atomic>
#include <cstdint>
#include <memory>

namespace ku {

struct ValidationMessageCounts {
    uint64_t warnings = 0;
    uint64_t errors = 0;
};

struct ValidationFeatureSelection {
    bool requested = false;
    bool validationEnabled = false;
    bool debugUtilsEnabled = false;
};

[[nodiscard]] constexpr ValidationFeatureSelection selectValidationFeatures(
    bool requested,
    bool validationLayerAvailable,
    bool debugUtilsAvailable) noexcept
{
    return {
        requested,
        requested && validationLayerAvailable,
        requested && validationLayerAvailable && debugUtilsAvailable,
    };
}

class ValidationMessageTracker final {
public:
    void record(VkDebugUtilsMessageSeverityFlagBitsEXT severity) noexcept;
    [[nodiscard]] ValidationMessageCounts counts() const noexcept;
    void reset() noexcept;

private:
    std::atomic<uint64_t> m_warningCount{0};
    std::atomic<uint64_t> m_errorCount{0};
};

class VulkanValidationState final {
public:
    explicit VulkanValidationState(
        bool requested,
        std::shared_ptr<ValidationMessageTracker> messages = {});

    void configureCapabilities(
        bool validationLayerAvailable,
        bool debugUtilsAvailable) noexcept;
    void setMessageCaptureEnabled(bool enabled) noexcept;

    [[nodiscard]] bool requested() const noexcept { return m_features.requested; }
    [[nodiscard]] bool validationEnabled() const noexcept {
        return m_features.validationEnabled;
    }
    [[nodiscard]] bool debugUtilsEnabled() const noexcept {
        return m_features.debugUtilsEnabled;
    }
    [[nodiscard]] bool messageCaptureEnabled() const noexcept {
        return m_messageCaptureEnabled;
    }

    void record(VkDebugUtilsMessageSeverityFlagBitsEXT severity) noexcept;
    [[nodiscard]] ValidationMessageCounts counts() const noexcept;
    void reset() noexcept;

private:
    ValidationFeatureSelection m_features;
    bool m_messageCaptureEnabled = false;
    std::shared_ptr<ValidationMessageTracker> m_messages;
};

[[nodiscard]] VkDebugUtilsMessengerCreateInfoEXT makeDebugMessengerCreateInfo(
    VulkanValidationState* state) noexcept;

VKAPI_ATTR VkBool32 VKAPI_CALL vulkanValidationCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT messageTypes,
    const VkDebugUtilsMessengerCallbackDataEXT* callbackData,
    void* userData) noexcept;

} // namespace ku
