#include "RHIInstance.h"
#include "../Core/Log.h"
#include "../Core/RuntimeError.h"

#include <GLFW/glfw3.h>

#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace ku {
namespace {

bool validationLayerAvailable(const char* layerName)
{
    std::vector<VkLayerProperties> properties;
    for (;;) {
        uint32_t count = 0;
        VK_CHECK(vkEnumerateInstanceLayerProperties(&count, nullptr));
        properties.resize(count);
        if (count == 0) {
            break;
        }

        const VkResult result = vkEnumerateInstanceLayerProperties(
            &count,
            properties.data());
        if (result == VK_INCOMPLETE) {
            continue;
        }
        if (result != VK_SUCCESS) {
            throw std::runtime_error(
                std::string("Failed to enumerate Vulkan instance layers: ")
                + vkResultToString(result));
        }
        properties.resize(count);
        break;
    }

    return std::ranges::any_of(
        properties,
        [layerName](const VkLayerProperties& property) {
            return std::strcmp(property.layerName, layerName) == 0;
        });
}

bool instanceExtensionAvailable(const char* extensionName)
{
    std::vector<VkExtensionProperties> properties;
    for (;;) {
        uint32_t count = 0;
        VK_CHECK(vkEnumerateInstanceExtensionProperties(nullptr, &count, nullptr));
        properties.resize(count);
        if (count == 0) {
            break;
        }

        const VkResult result = vkEnumerateInstanceExtensionProperties(
            nullptr,
            &count,
            properties.data());
        if (result == VK_INCOMPLETE) {
            continue;
        }
        if (result != VK_SUCCESS) {
            throw std::runtime_error(
                std::string("Failed to enumerate Vulkan instance extensions: ")
                + vkResultToString(result));
        }
        properties.resize(count);
        break;
    }

    return std::ranges::any_of(
        properties,
        [extensionName](const VkExtensionProperties& property) {
            return std::strcmp(property.extensionName, extensionName) == 0;
        });
}

} // namespace

RHIInstance::RHIInstance(
    const char* appName,
    uint32_t appVersion,
    std::shared_ptr<ValidationMessageTracker> validationMessages)
    : m_validationState(KU_DEBUG_BUILD != 0, std::move(validationMessages))
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
        throw RuntimeUnavailableError(
            "GLFW did not provide required Vulkan instance extensions");
    }

    std::vector<const char*> enabledExtensions(glfwExts, glfwExts + glfwExtCount);
    bool debugUtilsEnabled = false;

#if KU_DEBUG_BUILD
    const bool layerAvailable = validationLayerAvailable(VALIDATION_LAYERS.front());
    const bool debugUtilsAvailable = layerAvailable
        && instanceExtensionAvailable(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    m_validationState.configureCapabilities(layerAvailable, debugUtilsAvailable);

    if (validationEnabled()) {
        debugUtilsEnabled = m_validationState.debugUtilsEnabled();
        if (debugUtilsEnabled) {
            const bool alreadyRequested = std::ranges::any_of(
                enabledExtensions,
                [](const char* extension) {
                    return std::strcmp(extension, VK_EXT_DEBUG_UTILS_EXTENSION_NAME) == 0;
                });
            if (!alreadyRequested) {
                enabledExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
            }
        } else {
            KU_WARN(
                "Vulkan validation layer is available, but {} is unavailable; "
                "validation messages cannot be captured by KuEngine",
                VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }
    } else {
        KU_WARN(
            "Vulkan validation requested for this Debug build, but {} is unavailable; "
            "continuing with validation disabled",
            VALIDATION_LAYERS.front());
    }
#endif

    VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
    if (debugUtilsEnabled) {
        debugCreateInfo = makeDebugMessengerCreateInfo(&m_validationState);
    }

    VkInstanceCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    info.pApplicationInfo = &appInfo;
    info.enabledExtensionCount = static_cast<uint32_t>(enabledExtensions.size());
    info.ppEnabledExtensionNames = enabledExtensions.data();
    if (validationEnabled()) {
        info.enabledLayerCount = static_cast<uint32_t>(VALIDATION_LAYERS.size());
        info.ppEnabledLayerNames = VALIDATION_LAYERS.data();
    }
    if (debugUtilsEnabled) {
        info.pNext = &debugCreateInfo;
    }

    const VkResult instanceResult = vkCreateInstance(&info, nullptr, &m_instance);
    if (instanceResult == VK_ERROR_INCOMPATIBLE_DRIVER) {
        throw RuntimeUnavailableError(
            "No Vulkan driver compatible with the requested Vulkan 1.3 API is available");
    }
    if (instanceResult != VK_SUCCESS) {
        throw std::runtime_error(
            std::string("Failed to create Vulkan instance: ")
            + vkResultToString(instanceResult));
    }

    if (debugUtilsEnabled) {
        const auto createMessenger = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(m_instance, "vkCreateDebugUtilsMessengerEXT"));
        if (!createMessenger) {
            vkDestroyInstance(m_instance, nullptr);
            m_instance = VK_NULL_HANDLE;
            throw std::runtime_error(
                "VK_EXT_debug_utils was enabled, but vkCreateDebugUtilsMessengerEXT is unavailable");
        }

        const VkResult result = createMessenger(
            m_instance,
            &debugCreateInfo,
            nullptr,
            &m_debugMessenger);
        if (result != VK_SUCCESS) {
            vkDestroyInstance(m_instance, nullptr);
            m_instance = VK_NULL_HANDLE;
            throw std::runtime_error(
                std::string("Failed to create Vulkan debug messenger: ")
                + vkResultToString(result));
        }
        m_validationState.setMessageCaptureEnabled(true);
    }

    KU_INFO(
        "Vulkan instance created (validation requested: {}, enabled: {}, message capture: {})",
        validationRequested(),
        validationEnabled(),
        validationMessageCaptureEnabled());
}

RHIInstance::~RHIInstance()
{
    if (m_debugMessenger != VK_NULL_HANDLE && m_instance != VK_NULL_HANDLE) {
        const auto destroyMessenger = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(m_instance, "vkDestroyDebugUtilsMessengerEXT"));
        if (destroyMessenger) {
            destroyMessenger(m_instance, m_debugMessenger, nullptr);
        } else {
            KU_WARN("Unable to resolve vkDestroyDebugUtilsMessengerEXT during shutdown");
        }
        m_debugMessenger = VK_NULL_HANDLE;
        m_validationState.setMessageCaptureEnabled(false);
    }

    if (m_instance != VK_NULL_HANDLE) {
        vkDestroyInstance(m_instance, nullptr);
        m_instance = VK_NULL_HANDLE;
        KU_INFO("Vulkan instance destroyed");
    }
}

VkSurfaceKHR RHIInstance::createSurface(GLFWwindow* window) const
{
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkResult result = glfwCreateWindowSurface(m_instance, window, nullptr, &surface);
    if (result != VK_SUCCESS) {
        throw RuntimeUnavailableError("Failed to create window surface");
    }
    return surface;
}

bool RHIInstance::submitValidationTestErrorForSmokeTest() const
{
    if (!validationMessageCaptureEnabled() || m_instance == VK_NULL_HANDLE) {
        return false;
    }

    const auto submitMessage = reinterpret_cast<PFN_vkSubmitDebugUtilsMessageEXT>(
        vkGetInstanceProcAddr(m_instance, "vkSubmitDebugUtilsMessageEXT"));
    if (!submitMessage) {
        return false;
    }

    VkDebugUtilsMessengerCallbackDataEXT callbackData{};
    callbackData.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CALLBACK_DATA_EXT;
    callbackData.pMessageIdName = "KuEngine-SmokeValidationInjection";
    callbackData.pMessage =
        "Controlled Validation ERROR injected by the KuEngine smoke runner";
    submitMessage(
        m_instance,
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
        VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT,
        &callbackData);
    return true;
}

} // namespace ku
