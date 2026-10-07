#include "RHIPipeline.h"
#include "RHIDevice.h"
#include "RHIShader.h"
#include "../Core/Log.h"

#include <algorithm>
#include <array>
#include <limits>
#include <stdexcept>

namespace ku {

void validatePushConstantRanges(
    std::span<const VkPushConstantRange> ranges,
    uint32_t maxPushConstantsSize)
{
    for (size_t i = 0; i < ranges.size(); ++i) {
        const VkPushConstantRange& range = ranges[i];
        if (range.stageFlags == 0 || range.size == 0
            || (range.offset % 4) != 0 || (range.size % 4) != 0
            || range.offset > maxPushConstantsSize
            || range.size > maxPushConstantsSize - range.offset) {
            throw std::invalid_argument(
                "Pipeline push constant range is invalid or exceeds the device limit");
        }
        for (size_t j = 0; j < i; ++j) {
            const VkPushConstantRange& other = ranges[j];
            const uint64_t begin = range.offset;
            const uint64_t end = begin + range.size;
            const uint64_t otherBegin = other.offset;
            const uint64_t otherEnd = otherBegin + other.size;
            if (begin < otherEnd && otherBegin < end
                && (range.stageFlags & other.stageFlags) != 0) {
                throw std::invalid_argument(
                    "Overlapping push constant ranges cannot share shader stages");
            }
        }
    }
}

void validatePipelineLayoutLimits(
    size_t descriptorSetCount,
    std::span<const VkPushConstantRange> ranges,
    const VkPhysicalDeviceLimits& limits,
    VkShaderStageFlags allowedStages)
{
    if (descriptorSetCount > limits.maxBoundDescriptorSets) {
        throw std::invalid_argument(
            "Pipeline layout exceeds maxBoundDescriptorSets");
    }
    validatePushConstantRanges(ranges, limits.maxPushConstantsSize);
    for (const VkPushConstantRange& range : ranges) {
        if ((range.stageFlags & ~allowedStages) != 0) {
            throw std::invalid_argument(
                "Pipeline push constant range uses an incompatible shader stage");
        }
    }
}

void validateGraphicsShaderStages(
    std::span<const VkShaderStageFlagBits> stages)
{
    bool hasVertex = false;
    bool hasFragment = false;
    for (const VkShaderStageFlagBits stage : stages) {
        bool* present = nullptr;
        if (stage == VK_SHADER_STAGE_VERTEX_BIT) present = &hasVertex;
        else if (stage == VK_SHADER_STAGE_FRAGMENT_BIT) present = &hasFragment;
        else {
            throw std::invalid_argument(
                "GraphicsPipelineDesc contains an unsupported shader stage");
        }
        if (*present) {
            throw std::invalid_argument(
                "GraphicsPipelineDesc contains a duplicate shader stage");
        }
        *present = true;
    }
    if (!hasVertex || !hasFragment || stages.size() != 2) {
        throw std::invalid_argument(
            "GraphicsPipelineDesc requires exactly one vertex and one fragment shader");
    }
}

void validateDispatchCount(
    uint32_t groupCountX,
    uint32_t groupCountY,
    uint32_t groupCountZ,
    const VkPhysicalDeviceLimits& limits)
{
    if (groupCountX == 0 || groupCountY == 0 || groupCountZ == 0
        || groupCountX > limits.maxComputeWorkGroupCount[0]
        || groupCountY > limits.maxComputeWorkGroupCount[1]
        || groupCountZ > limits.maxComputeWorkGroupCount[2]) {
        throw std::invalid_argument(
            "Compute dispatch group count is zero or exceeds the device limit");
    }
}

void validateComputeParameterSetBinding(
    std::span<const VkDescriptorSetLayout> pipelineLayouts,
    uint32_t setIndex,
    VkDescriptorSetLayout parameterLayout,
    VkDescriptorSet parameterSet)
{
    if (setIndex >= pipelineLayouts.size()
        || parameterLayout == VK_NULL_HANDLE
        || parameterSet == VK_NULL_HANDLE
        || pipelineLayouts[setIndex] != parameterLayout) {
        throw std::invalid_argument(
            "Compute parameter set does not match the pipeline set layout");
    }
}

RHIPipeline::RHIPipeline(const RHIDevice& device, const GraphicsPipelineDesc& desc)
    : m_device(device.device())
    , m_descriptorSetCount(static_cast<uint32_t>(desc.descriptorSetLayouts.size()))
{
    std::vector<VkShaderStageFlagBits> shaderStages;
    shaderStages.reserve(desc.shaders.size());
    for (const auto shader : desc.shaders) shaderStages.push_back(shader.get().stage());
    validateGraphicsShaderStages(shaderStages);
    validatePipelineLayoutLimits(
        desc.descriptorSetLayouts.size(), desc.pushConstantRanges,
        device.properties().limits,
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);

    std::array<VkPipelineShaderStageCreateInfo, 2> stages{};
    for (const auto shaderRef : desc.shaders) {
        const RHIShader& shader = shaderRef.get();
        const size_t index = shader.stage() == VK_SHADER_STAGE_VERTEX_BIT ? 0 : 1;
        stages[index].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stages[index].stage = shader.stage();
        stages[index].module = shader.module();
        stages[index].pName = shader.entryPoint().c_str();
    }

    VkPipelineVertexInputStateCreateInfo vertexInput{};
    vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInput.vertexBindingDescriptionCount = static_cast<uint32_t>(desc.vertexBindings.size());
    vertexInput.pVertexBindingDescriptions =
        desc.vertexBindings.empty() ? nullptr : desc.vertexBindings.data();
    vertexInput.vertexAttributeDescriptionCount = static_cast<uint32_t>(desc.vertexAttributes.size());
    vertexInput.pVertexAttributeDescriptions =
        desc.vertexAttributes.empty() ? nullptr : desc.vertexAttributes.data();

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = desc.topology;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = desc.cullMode;
    rasterizer.frontFace = desc.frontFace;
    rasterizer.depthBiasEnable = VK_FALSE;

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.rasterizationSamples = desc.samples;
    multisampling.sampleShadingEnable = VK_FALSE;

    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = desc.depthTest ? VK_TRUE : VK_FALSE;
    depthStencil.depthWriteEnable = desc.depthWrite ? VK_TRUE : VK_FALSE;
    depthStencil.depthCompareOp = desc.depthCompareOp;
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.stencilTestEnable = VK_FALSE;

    if (desc.colorFormats.empty() && desc.depthFormat == VK_FORMAT_UNDEFINED) {
        throw std::invalid_argument(
            "GraphicsPipelineDesc requires at least one color or depth attachment format");
    }
    if (std::any_of(
            desc.colorFormats.begin(),
            desc.colorFormats.end(),
            [](VkFormat format) {
                return format == VK_FORMAT_UNDEFINED;
            })) {
        throw std::invalid_argument(
            "GraphicsPipelineDesc color attachment format cannot be VK_FORMAT_UNDEFINED");
    }
    if ((desc.depthTest || desc.depthWrite)
        && desc.depthFormat == VK_FORMAT_UNDEFINED) {
        throw std::invalid_argument(
            "GraphicsPipelineDesc depth test/write requires a depth attachment format");
    }

    const std::vector<VkFormat>& colorFormats = desc.colorFormats;
    std::vector<VkPipelineColorBlendAttachmentState> blendAttachments(colorFormats.size());
    for (auto& attachment : blendAttachments) {
        attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                    VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        attachment.blendEnable = desc.blendEnable ? VK_TRUE : VK_FALSE;
        attachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        attachment.colorBlendOp = VK_BLEND_OP_ADD;
        attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        attachment.alphaBlendOp = VK_BLEND_OP_ADD;
    }

    VkPipelineColorBlendStateCreateInfo colorBlend{};
    colorBlend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlend.logicOpEnable = VK_FALSE;
    colorBlend.attachmentCount = static_cast<uint32_t>(blendAttachments.size());
    colorBlend.pAttachments = blendAttachments.data();

    std::array<VkDynamicState, 2> dynamicStates = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR,
    };
    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
    dynamicState.pDynamicStates = dynamicStates.data();

    VkPipelineLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.setLayoutCount = static_cast<uint32_t>(desc.descriptorSetLayouts.size());
    layoutInfo.pSetLayouts =
        desc.descriptorSetLayouts.empty() ? nullptr : desc.descriptorSetLayouts.data();
    layoutInfo.pushConstantRangeCount = static_cast<uint32_t>(desc.pushConstantRanges.size());
    layoutInfo.pPushConstantRanges =
        desc.pushConstantRanges.empty() ? nullptr : desc.pushConstantRanges.data();
    VK_CHECK(vkCreatePipelineLayout(m_device, &layoutInfo, nullptr, &m_layout));

    VkPipelineRenderingCreateInfo renderingInfo{};
    renderingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    renderingInfo.colorAttachmentCount = static_cast<uint32_t>(colorFormats.size());
    renderingInfo.pColorAttachmentFormats = colorFormats.data();
    renderingInfo.depthAttachmentFormat = desc.depthFormat;
    renderingInfo.stencilAttachmentFormat = VK_FORMAT_UNDEFINED;

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.pNext = &renderingInfo;
    pipelineInfo.stageCount = static_cast<uint32_t>(stages.size());
    pipelineInfo.pStages = stages.data();
    pipelineInfo.pVertexInputState = &vertexInput;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.pColorBlendState = &colorBlend;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = m_layout;
    pipelineInfo.renderPass = VK_NULL_HANDLE;
    pipelineInfo.subpass = 0;

    try {
        VK_CHECK(vkCreateGraphicsPipelines(
            m_device,
            VK_NULL_HANDLE,
            1,
            &pipelineInfo,
            nullptr,
            &m_pipeline));
    } catch (...) {
        // A throwing constructor does not run RHIPipeline::~RHIPipeline().
        // Release any returned pipeline and the layout before propagating.
        if (m_pipeline != VK_NULL_HANDLE) {
            vkDestroyPipeline(m_device, m_pipeline, nullptr);
            m_pipeline = VK_NULL_HANDLE;
        }
        vkDestroyPipelineLayout(m_device, m_layout, nullptr);
        m_layout = VK_NULL_HANDLE;
        throw;
    }

    KU_INFO("Graphics pipeline created");
}

RHIPipeline::~RHIPipeline()
{
    if (m_pipeline) vkDestroyPipeline(m_device, m_pipeline, nullptr);
    if (m_layout) vkDestroyPipelineLayout(m_device, m_layout, nullptr);
}

void RHIPipeline::bind(VkCommandBuffer cmd) const
{
    if (m_pipeline == VK_NULL_HANDLE) {
        return;
    }

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);
}

RHIComputePipeline::RHIComputePipeline(
    const RHIDevice& device,
    const ComputePipelineDesc& desc)
    : m_device(device.device())
    , m_descriptorSetCount(
          static_cast<uint32_t>(desc.descriptorSetLayouts.size()))
    , m_descriptorSetLayouts(desc.descriptorSetLayouts)
{
    if (!device.graphicsQueueSupportsCompute()) {
        throw std::runtime_error(
            "Compute pipeline requires compute support on the graphics queue");
    }
    if (desc.shader == nullptr || !desc.shader->isValid()
        || desc.shader->stage() != VK_SHADER_STAGE_COMPUTE_BIT) {
        throw std::invalid_argument(
            "ComputePipelineDesc requires one valid compute shader");
    }
    validatePipelineLayoutLimits(
        desc.descriptorSetLayouts.size(), desc.pushConstantRanges,
        device.properties().limits, VK_SHADER_STAGE_COMPUTE_BIT);

    VkPipelineLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.setLayoutCount =
        static_cast<uint32_t>(desc.descriptorSetLayouts.size());
    layoutInfo.pSetLayouts = desc.descriptorSetLayouts.empty()
        ? nullptr : desc.descriptorSetLayouts.data();
    layoutInfo.pushConstantRangeCount =
        static_cast<uint32_t>(desc.pushConstantRanges.size());
    layoutInfo.pPushConstantRanges = desc.pushConstantRanges.empty()
        ? nullptr : desc.pushConstantRanges.data();
    VK_CHECK(vkCreatePipelineLayout(m_device, &layoutInfo, nullptr, &m_layout));

    VkPipelineShaderStageCreateInfo stage{};
    stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    stage.module = desc.shader->module();
    stage.pName = desc.shader->entryPoint().c_str();
    VkComputePipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.stage = stage;
    pipelineInfo.layout = m_layout;
    try {
        VK_CHECK(vkCreateComputePipelines(
            m_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_pipeline));
    } catch (...) {
        if (m_pipeline != VK_NULL_HANDLE) {
            vkDestroyPipeline(m_device, m_pipeline, nullptr);
            m_pipeline = VK_NULL_HANDLE;
        }
        vkDestroyPipelineLayout(m_device, m_layout, nullptr);
        m_layout = VK_NULL_HANDLE;
        throw;
    }
    KU_INFO("Compute pipeline created");
}

RHIComputePipeline::~RHIComputePipeline()
{
    if (m_pipeline) vkDestroyPipeline(m_device, m_pipeline, nullptr);
    if (m_layout) vkDestroyPipelineLayout(m_device, m_layout, nullptr);
}

void RHIComputePipeline::bind(VkCommandBuffer cmd) const
{
    if (m_pipeline != VK_NULL_HANDLE) {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipeline);
    }
}

VkDescriptorSetLayout RHIComputePipeline::descriptorSetLayout(
    uint32_t index) const
{
    if (index >= m_descriptorSetLayouts.size()) {
        throw std::out_of_range("Compute pipeline descriptor set index is out of range");
    }
    return m_descriptorSetLayouts[index];
}

} // namespace ku
