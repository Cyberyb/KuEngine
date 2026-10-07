#include "RHIParameterSet.h"

#include "RHICommon.h"
#include "RHIDevice.h"

#include <algorithm>
#include <array>
#include <map>
#include <stdexcept>

namespace ku {
namespace {

[[nodiscard]] bool supportedDescriptorType(VkDescriptorType type) noexcept
{
    return type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER
        || type == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER
        || type == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
        || type == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
}

[[nodiscard]] bool isBufferType(VkDescriptorType type) noexcept
{
    return type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER
        || type == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
}

[[nodiscard]] bool isImageType(VkDescriptorType type) noexcept
{
    return type == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
        || type == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
}

} // namespace

void validateParameterBindings(
    std::span<const ParameterBindingDesc> bindings,
    const VkPhysicalDeviceLimits& limits)
{
    if (bindings.empty()) {
        throw std::invalid_argument(
            "Parameter set requires at least one binding");
    }
    uint32_t uniformCount = 0;
    uint32_t storageBufferCount = 0;
    uint32_t sampledCount = 0;
    uint32_t storageImageCount = 0;
    struct StageCounts {
        uint32_t uniform = 0;
        uint32_t storageBuffer = 0;
        uint32_t sampled = 0;
        uint32_t storageImage = 0;
    } stageCounts[3]{};
    constexpr VkShaderStageFlagBits stages[] = {
        VK_SHADER_STAGE_VERTEX_BIT,
        VK_SHADER_STAGE_FRAGMENT_BIT,
        VK_SHADER_STAGE_COMPUTE_BIT};
    for (size_t i = 0; i < bindings.size(); ++i) {
        const ParameterBindingDesc& binding = bindings[i];
        if (!supportedDescriptorType(binding.type) || binding.stages == 0
            || (binding.stages
                & ~(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT
                    | VK_SHADER_STAGE_COMPUTE_BIT)) != 0) {
            throw std::invalid_argument(
                "Parameter binding has an unsupported type or shader stage");
        }
        if (std::any_of(
                bindings.begin(), bindings.begin() + i,
                [&](const ParameterBindingDesc& other) {
                    return other.binding == binding.binding;
                })) {
            throw std::invalid_argument("Parameter binding index is duplicated");
        }
        uniformCount += binding.type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        storageBufferCount += binding.type == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        sampledCount += binding.type == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        storageImageCount += binding.type == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        for (size_t stageIndex = 0; stageIndex < std::size(stages); ++stageIndex) {
            if ((binding.stages & stages[stageIndex]) == 0) continue;
            stageCounts[stageIndex].uniform +=
                binding.type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            stageCounts[stageIndex].storageBuffer +=
                binding.type == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            stageCounts[stageIndex].sampled +=
                binding.type == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            stageCounts[stageIndex].storageImage +=
                binding.type == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        }
    }
    if (uniformCount > limits.maxDescriptorSetUniformBuffers
        || storageBufferCount > limits.maxDescriptorSetStorageBuffers
        || sampledCount > limits.maxDescriptorSetSampledImages
        || sampledCount > limits.maxDescriptorSetSamplers
        || storageImageCount > limits.maxDescriptorSetStorageImages) {
        throw std::invalid_argument(
            "Parameter bindings exceed descriptor-set device limits");
    }
    for (const StageCounts& counts : stageCounts) {
        if (counts.uniform > limits.maxPerStageDescriptorUniformBuffers
            || counts.storageBuffer > limits.maxPerStageDescriptorStorageBuffers
            || counts.sampled > limits.maxPerStageDescriptorSampledImages
            || counts.sampled > limits.maxPerStageDescriptorSamplers
            || counts.storageImage > limits.maxPerStageDescriptorStorageImages) {
            throw std::invalid_argument(
                "Parameter bindings exceed per-stage descriptor device limits");
        }
    }
}

void validateBufferParameterWrite(
    const ParameterBindingDesc& binding,
    VkDeviceSize bufferSize,
    VkDeviceSize offset,
    VkDeviceSize range,
    const VkPhysicalDeviceLimits& limits)
{
    if (!isBufferType(binding.type) || bufferSize == 0 || range == 0
        || range == VK_WHOLE_SIZE || offset > bufferSize
        || range > bufferSize - offset) {
        throw std::invalid_argument("Buffer parameter write has an invalid type or range");
    }
    const bool uniform = binding.type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    const VkDeviceSize maxRange = uniform
        ? limits.maxUniformBufferRange : limits.maxStorageBufferRange;
    const VkDeviceSize alignment = uniform
        ? limits.minUniformBufferOffsetAlignment
        : limits.minStorageBufferOffsetAlignment;
    if (range > maxRange || (alignment != 0 && offset % alignment != 0)) {
        throw std::invalid_argument(
            "Buffer parameter range exceeds device range/alignment limits");
    }
}

void validateImageParameterWrite(
    const ParameterBindingDesc& binding,
    VkSampler sampler,
    VkImageView imageView,
    VkImageLayout layout)
{
    if (!isImageType(binding.type) || imageView == VK_NULL_HANDLE) {
        throw std::invalid_argument("Image parameter write has an invalid type or view");
    }
    if (binding.type == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER) {
        if (sampler == VK_NULL_HANDLE
            || (layout != VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                && layout != VK_IMAGE_LAYOUT_GENERAL)) {
            throw std::invalid_argument(
                "Combined sampler requires a sampler and readable image layout");
        }
    } else if (sampler != VK_NULL_HANDLE || layout != VK_IMAGE_LAYOUT_GENERAL) {
        throw std::invalid_argument(
            "Storage image requires no sampler and VK_IMAGE_LAYOUT_GENERAL");
    }
}

RHIParameterSet::RHIParameterSet(
    const RHIDevice& device,
    std::vector<ParameterBindingDesc> bindings)
    : m_device(device.device())
    , m_limits(device.properties().limits)
    , m_bindings(std::move(bindings))
{
    validateParameterBindings(m_bindings, m_limits);
    std::vector<VkDescriptorSetLayoutBinding> nativeBindings;
    nativeBindings.reserve(m_bindings.size());
    std::map<VkDescriptorType, uint32_t> typeCounts;
    for (const ParameterBindingDesc& binding : m_bindings) {
        nativeBindings.push_back({
            binding.binding, binding.type, 1, binding.stages, nullptr});
        ++typeCounts[binding.type];
    }

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(nativeBindings.size());
    layoutInfo.pBindings = nativeBindings.empty() ? nullptr : nativeBindings.data();
    VK_CHECK(vkCreateDescriptorSetLayout(m_device, &layoutInfo, nullptr, &m_layout));

    try {
        std::vector<VkDescriptorPoolSize> poolSizes;
        poolSizes.reserve(typeCounts.size());
        for (const auto& [type, count] : typeCounts) {
            poolSizes.push_back({type, count});
        }
        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.maxSets = 1;
        poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
        poolInfo.pPoolSizes = poolSizes.empty() ? nullptr : poolSizes.data();
        VK_CHECK(vkCreateDescriptorPool(m_device, &poolInfo, nullptr, &m_pool));

        VkDescriptorSetAllocateInfo allocateInfo{};
        allocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocateInfo.descriptorPool = m_pool;
        allocateInfo.descriptorSetCount = 1;
        allocateInfo.pSetLayouts = &m_layout;
        VK_CHECK(vkAllocateDescriptorSets(m_device, &allocateInfo, &m_set));
    } catch (...) {
        if (m_pool) vkDestroyDescriptorPool(m_device, m_pool, nullptr);
        if (m_layout) vkDestroyDescriptorSetLayout(m_device, m_layout, nullptr);
        m_pool = VK_NULL_HANDLE;
        m_layout = VK_NULL_HANDLE;
        throw;
    }
}

RHIParameterSet::~RHIParameterSet()
{
    if (m_pool) vkDestroyDescriptorPool(m_device, m_pool, nullptr);
    if (m_layout) vkDestroyDescriptorSetLayout(m_device, m_layout, nullptr);
}

const ParameterBindingDesc& RHIParameterSet::binding(uint32_t index) const
{
    const auto found = std::find_if(
        m_bindings.begin(), m_bindings.end(),
        [&](const ParameterBindingDesc& candidate) {
            return candidate.binding == index;
        });
    if (found == m_bindings.end()) {
        throw std::invalid_argument("Descriptor write references an undeclared binding");
    }
    return *found;
}

void RHIParameterSet::writeBuffer(
    uint32_t bindingIndex,
    VkBuffer buffer,
    VkDeviceSize bufferSize,
    VkDeviceSize offset,
    VkDeviceSize range)
{
    if (buffer == VK_NULL_HANDLE) {
        throw std::invalid_argument("Buffer parameter requires a valid buffer");
    }
    const ParameterBindingDesc& desc = binding(bindingIndex);
    validateBufferParameterWrite(desc, bufferSize, offset, range, m_limits);
    VkDescriptorBufferInfo info{buffer, offset, range};
    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = m_set;
    write.dstBinding = bindingIndex;
    write.descriptorCount = 1;
    write.descriptorType = desc.type;
    write.pBufferInfo = &info;
    vkUpdateDescriptorSets(m_device, 1, &write, 0, nullptr);
}

void RHIParameterSet::writeImage(
    uint32_t bindingIndex,
    VkSampler sampler,
    VkImageView imageView,
    VkImageLayout layout)
{
    const ParameterBindingDesc& desc = binding(bindingIndex);
    validateImageParameterWrite(desc, sampler, imageView, layout);
    VkDescriptorImageInfo info{sampler, imageView, layout};
    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = m_set;
    write.dstBinding = bindingIndex;
    write.descriptorCount = 1;
    write.descriptorType = desc.type;
    write.pImageInfo = &info;
    vkUpdateDescriptorSets(m_device, 1, &write, 0, nullptr);
}

} // namespace ku
