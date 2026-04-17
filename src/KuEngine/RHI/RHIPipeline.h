#pragma once

#include "RHICommon.h"
#include <vulkan/vulkan.h>
#include <vector>
#include <memory>
#include <span>

namespace ku {

class RHIDevice;
class RHIShader;

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
    bool                          depthTest = true;
    bool                          depthWrite = true;
    bool                          blendEnable = false;
};

class RHIPipeline {
public:
    RHIPipeline() = default;
    RHIPipeline(const RHIDevice& device, const GraphicsPipelineDesc& desc);
    ~RHIPipeline();

    [[nodiscard]] VkPipeline pipeline() const { return m_pipeline; }
    [[nodiscard]] VkPipelineLayout layout() const { return m_layout; }

    void bind(VkCommandBuffer cmd) const;

private:
    VkPipeline      m_pipeline = VK_NULL_HANDLE;
    VkPipelineLayout m_layout = VK_NULL_HANDLE;
    VkDevice        m_device = VK_NULL_HANDLE;
};

} // namespace ku
