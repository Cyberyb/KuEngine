// KuEngine RHI 图形管线模块：根据着色器与渲染状态创建并持有 Vulkan 管线及布局。
#pragma once

#include "RHICommon.h"
#include <vulkan/vulkan.h>
#include <vector>
#include <memory>
#include <span>

namespace ku {

class RHIDevice;
class RHIShader;

void validatePushConstantRanges(
    std::span<const VkPushConstantRange> ranges,
    uint32_t maxPushConstantsSize);
void validatePipelineLayoutLimits(
    size_t descriptorSetCount,
    std::span<const VkPushConstantRange> ranges,
    const VkPhysicalDeviceLimits& limits,
    VkShaderStageFlags allowedStages);
void validateGraphicsShaderStages(
    std::span<const VkShaderStageFlagBits> stages);
void validateDispatchCount(
    uint32_t groupCountX,
    uint32_t groupCountY,
    uint32_t groupCountZ,
    const VkPhysicalDeviceLimits& limits);
void validateComputeParameterSetBinding(
    std::span<const VkDescriptorSetLayout> pipelineLayouts,
    uint32_t setIndex,
    VkDescriptorSetLayout parameterLayout,
    VkDescriptorSet parameterSet);

struct GraphicsPipelineDesc {
    std::vector<std::reference_wrapper<RHIShader>> shaders;
    std::vector<VkFormat>         colorFormats;
    std::vector<VkVertexInputBindingDescription> vertexBindings;
    std::vector<VkVertexInputAttributeDescription> vertexAttributes;
    std::vector<VkDescriptorSetLayout> descriptorSetLayouts;
    std::vector<VkPushConstantRange> pushConstantRanges;
    VkFormat                      depthFormat = VK_FORMAT_UNDEFINED;
    VkSampleCountFlagBits          samples = VK_SAMPLE_COUNT_1_BIT;
    VkPrimitiveTopology            topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    VkCullModeFlags               cullMode = VK_CULL_MODE_BACK_BIT;
    VkFrontFace                   frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    VkCompareOp                    depthCompareOp = VK_COMPARE_OP_LESS;
    bool                          depthTest = true;
    bool                          depthWrite = true;
    bool                          blendEnable = false;
};

struct ComputePipelineDesc {
    RHIShader* shader = nullptr;
    std::vector<VkDescriptorSetLayout> descriptorSetLayouts;
    std::vector<VkPushConstantRange> pushConstantRanges;
};

class RHIPipeline {
public:
    RHIPipeline() = default;
    RHIPipeline(const RHIDevice& device, const GraphicsPipelineDesc& desc);
    ~RHIPipeline();

    [[nodiscard]] VkPipeline pipeline() const { return m_pipeline; }
    [[nodiscard]] VkPipelineLayout layout() const { return m_layout; }
    [[nodiscard]] uint32_t descriptorSetCount() const
    {
        return m_descriptorSetCount;
    }

    void bind(VkCommandBuffer cmd) const;

private:
    VkPipeline      m_pipeline = VK_NULL_HANDLE;
    VkPipelineLayout m_layout = VK_NULL_HANDLE;
    VkDevice        m_device = VK_NULL_HANDLE;
    uint32_t        m_descriptorSetCount = 0;
};

class RHIComputePipeline {
public:
    RHIComputePipeline() = default;
    RHIComputePipeline(const RHIDevice& device, const ComputePipelineDesc& desc);
    ~RHIComputePipeline();

    RHIComputePipeline(const RHIComputePipeline&) = delete;
    RHIComputePipeline& operator=(const RHIComputePipeline&) = delete;

    [[nodiscard]] VkPipeline pipeline() const noexcept { return m_pipeline; }
    [[nodiscard]] VkPipelineLayout layout() const noexcept { return m_layout; }
    [[nodiscard]] uint32_t descriptorSetCount() const noexcept
    {
        return m_descriptorSetCount;
    }
    [[nodiscard]] VkDescriptorSetLayout descriptorSetLayout(
        uint32_t index) const;
    [[nodiscard]] std::span<const VkDescriptorSetLayout>
    descriptorSetLayouts() const noexcept { return m_descriptorSetLayouts; }
    void bind(VkCommandBuffer cmd) const;

private:
    VkPipeline m_pipeline = VK_NULL_HANDLE;
    VkPipelineLayout m_layout = VK_NULL_HANDLE;
    VkDevice m_device = VK_NULL_HANDLE;
    uint32_t m_descriptorSetCount = 0;
    std::vector<VkDescriptorSetLayout> m_descriptorSetLayouts;
};

} // namespace ku
