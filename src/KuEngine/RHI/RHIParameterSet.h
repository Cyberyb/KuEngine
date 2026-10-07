#pragma once

#include <span>
#include <vector>

#include <vulkan/vulkan.h>

namespace ku {

class RHIDevice;

struct ParameterBindingDesc {
    uint32_t binding = 0;
    VkDescriptorType type = VK_DESCRIPTOR_TYPE_MAX_ENUM;
    VkShaderStageFlags stages = 0;

    auto operator<=>(const ParameterBindingDesc&) const = default;
};

void validateParameterBindings(
    std::span<const ParameterBindingDesc> bindings,
    const VkPhysicalDeviceLimits& limits);
void validateBufferParameterWrite(
    const ParameterBindingDesc& binding,
    VkDeviceSize bufferSize,
    VkDeviceSize offset,
    VkDeviceSize range,
    const VkPhysicalDeviceLimits& limits);
void validateImageParameterWrite(
    const ParameterBindingDesc& binding,
    VkSampler sampler,
    VkImageView imageView,
    VkImageLayout layout);

class RHIParameterSet {
public:
    RHIParameterSet() = default;
    RHIParameterSet(
        const RHIDevice& device,
        std::vector<ParameterBindingDesc> bindings);
    ~RHIParameterSet();

    RHIParameterSet(const RHIParameterSet&) = delete;
    RHIParameterSet& operator=(const RHIParameterSet&) = delete;

    [[nodiscard]] VkDescriptorSetLayout layout() const noexcept { return m_layout; }
    [[nodiscard]] VkDescriptorSet set() const noexcept { return m_set; }
    [[nodiscard]] const std::vector<ParameterBindingDesc>& bindings() const noexcept
    {
        return m_bindings;
    }

    void writeBuffer(
        uint32_t binding,
        VkBuffer buffer,
        VkDeviceSize bufferSize,
        VkDeviceSize offset,
        VkDeviceSize range);
    void writeImage(
        uint32_t binding,
        VkSampler sampler,
        VkImageView imageView,
        VkImageLayout layout);

private:
    [[nodiscard]] const ParameterBindingDesc& binding(uint32_t index) const;

    VkDevice m_device = VK_NULL_HANDLE;
    VkPhysicalDeviceLimits m_limits{};
    VkDescriptorSetLayout m_layout = VK_NULL_HANDLE;
    VkDescriptorPool m_pool = VK_NULL_HANDLE;
    VkDescriptorSet m_set = VK_NULL_HANDLE;
    std::vector<ParameterBindingDesc> m_bindings;
};

} // namespace ku
