#include "VulkanValidation.h"
#include "../Core/Log.h"

#include <utility>

namespace ku {
namespace {

const char* messageTypeLabel(VkDebugUtilsMessageTypeFlagsEXT messageTypes) noexcept
{
    if ((messageTypes & VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT) != 0) {
        return "validation";
    }
    if ((messageTypes & VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT) != 0) {
        return "performance";
    }
    return "general";
}

} // namespace

VulkanValidationState::VulkanValidationState(
    bool requested,
    std::shared_ptr<ValidationMessageTracker> messages)
    : m_features(selectValidationFeatures(requested, false, false)),
      m_messages(messages ? std::move(messages) : std::make_shared<ValidationMessageTracker>())
{
}

void VulkanValidationState::configureCapabilities(
    bool validationLayerAvailable,
    bool debugUtilsAvailable) noexcept
{
    m_features = selectValidationFeatures(
        m_features.requested,
        validationLayerAvailable,
        debugUtilsAvailable);
    m_messageCaptureEnabled = false;
}

void VulkanValidationState::setMessageCaptureEnabled(bool enabled) noexcept
{
    m_messageCaptureEnabled = enabled && m_features.debugUtilsEnabled;
}

void VulkanValidationState::record(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity) noexcept
{
    m_messages->record(severity);
}

ValidationMessageCounts VulkanValidationState::counts() const noexcept
{
    return m_messages->counts();
}

void VulkanValidationState::reset() noexcept
{
    m_messages->reset();
}

void ValidationMessageTracker::record(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity) noexcept
{
    if ((severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) != 0) {
        m_errorCount.fetch_add(1, std::memory_order_relaxed);
    } else if ((severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) != 0) {
        m_warningCount.fetch_add(1, std::memory_order_relaxed);
    }
}

ValidationMessageCounts ValidationMessageTracker::counts() const noexcept
{
    return {
        m_warningCount.load(std::memory_order_relaxed),
        m_errorCount.load(std::memory_order_relaxed),
    };
}

void ValidationMessageTracker::reset() noexcept
{
    m_warningCount.store(0, std::memory_order_relaxed);
    m_errorCount.store(0, std::memory_order_relaxed);
}

VkDebugUtilsMessengerCreateInfoEXT makeDebugMessengerCreateInfo(
    VulkanValidationState* state) noexcept
{
    VkDebugUtilsMessengerCreateInfoEXT info{};
    info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    info.messageSeverity =
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT
        | VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT
        | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT
        | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    info.messageType =
        VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT
        | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
        | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    info.pfnUserCallback = &vulkanValidationCallback;
    info.pUserData = state;
    return info;
}

VKAPI_ATTR VkBool32 VKAPI_CALL vulkanValidationCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT messageTypes,
    const VkDebugUtilsMessengerCallbackDataEXT* callbackData,
    void* userData) noexcept
{
    auto* state = static_cast<VulkanValidationState*>(userData);
    if (state) {
        state->record(severity);
    }

    const char* message = callbackData && callbackData->pMessage
        ? callbackData->pMessage
        : "Vulkan validation callback did not provide a message";
    const char* messageId = callbackData && callbackData->pMessageIdName
        ? callbackData->pMessageIdName
        : "unknown";
    const char* type = messageTypeLabel(messageTypes);

    try {
        if ((severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) != 0) {
            KU_ERROR(
                "KUENGINE_VALIDATION_ERROR Vulkan {} [{}]: {}",
                type,
                messageId,
                message);
        } else if ((severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) != 0) {
            KU_WARN("Vulkan {} [{}]: {}", type, messageId, message);
        } else if ((severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) != 0) {
            KU_INFO("Vulkan {} [{}]: {}", type, messageId, message);
        } else {
            KU_TRACE("Vulkan {} [{}]: {}", type, messageId, message);
        }
    } catch (...) {
        // Vulkan callbacks must not allow logging failures to cross the C ABI boundary.
    }

    return VK_FALSE;
}

} // namespace ku
