#include "ForwardDisplayProgram.h"

#include <KuEngine/Core/Log.h>
#include <KuEngine/RHI/CommandList.h>
#include <KuEngine/RHI/RHIDevice.h>
#include <KuEngine/RHI/RHIParameterSet.h>
#include <KuEngine/RHI/RHIPipeline.h>
#include <KuEngine/RHI/RHIShader.h>

#include <array>
#include <filesystem>
#include <stdexcept>

namespace ku {
namespace {

struct DisplayPushConstants {
    std::array<float, 2> uvScale{};
    std::array<float, 2> uvOffset{};
};

} // namespace

ForwardDisplayProgram::ForwardDisplayProgram() = default;

ForwardDisplayProgram::~ForwardDisplayProgram()
{
    reset();
}

void ForwardDisplayProgram::reset() noexcept
{
    m_pipeline.reset();
    m_fragmentShader.reset();
    m_vertexShader.reset();
    m_parameters.reset();
    if (m_sampler != VK_NULL_HANDLE && m_device != nullptr) {
        vkDestroySampler(m_device->device(), m_sampler, nullptr);
    }
    m_sampler = VK_NULL_HANDLE;
    m_device = nullptr;
    m_sceneColorFormat = VK_FORMAT_UNDEFINED;
    m_boundGeneration = 0;
    m_boundView = VK_NULL_HANDLE;
    m_descriptorRebindCount = 0;
}

void ForwardDisplayProgram::initialize(
    const RHIDevice& device,
    VkFormat sceneColorFormat,
    VkFormat outputFormat)
{
    if (sceneColorFormat == VK_FORMAT_UNDEFINED
        || outputFormat == VK_FORMAT_UNDEFINED) {
        throw std::invalid_argument(
            "Forward display requires defined source and output formats");
    }

    VkSampler candidateSampler = VK_NULL_HANDLE;
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_NEAREST;
    samplerInfo.minFilter = VK_FILTER_NEAREST;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.maxLod = 0.0f;
    VK_CHECK(vkCreateSampler(
        device.device(), &samplerInfo, nullptr, &candidateSampler));

    try {
        auto candidateParameters = std::make_unique<RHIParameterSet>(
            device,
            std::vector<ParameterBindingDesc>{
                {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                 VK_SHADER_STAGE_FRAGMENT_BIT},
            });
        const std::filesystem::path shaderDirectory =
            std::filesystem::current_path() / "shaders" / "forward";
        auto candidateVertex = std::make_unique<RHIShader>(device, ShaderDesc{
            shaderDirectory / "display.vert.spv",
            VK_SHADER_STAGE_VERTEX_BIT,
            "main"});
        auto candidateFragment = std::make_unique<RHIShader>(device, ShaderDesc{
            shaderDirectory / "display.frag.spv",
            VK_SHADER_STAGE_FRAGMENT_BIT,
            "main"});

        GraphicsPipelineDesc pipelineDesc{};
        pipelineDesc.shaders = {*candidateVertex, *candidateFragment};
        pipelineDesc.colorFormats = {outputFormat};
        pipelineDesc.descriptorSetLayouts = {candidateParameters->layout()};
        pipelineDesc.pushConstantRanges = {{
            VK_SHADER_STAGE_FRAGMENT_BIT,
            0,
            sizeof(DisplayPushConstants)}};
        pipelineDesc.depthFormat = VK_FORMAT_UNDEFINED;
        pipelineDesc.depthTest = false;
        pipelineDesc.depthWrite = false;
        pipelineDesc.cullMode = VK_CULL_MODE_NONE;
        auto candidatePipeline =
            std::make_unique<RHIPipeline>(device, pipelineDesc);

        reset();
        m_device = &device;
        m_sceneColorFormat = sceneColorFormat;
        m_sampler = candidateSampler;
        candidateSampler = VK_NULL_HANDLE;
        m_parameters = std::move(candidateParameters);
        m_vertexShader = std::move(candidateVertex);
        m_fragmentShader = std::move(candidateFragment);
        m_pipeline = std::move(candidatePipeline);
    } catch (...) {
        if (candidateSampler != VK_NULL_HANDLE) {
            vkDestroySampler(device.device(), candidateSampler, nullptr);
        }
        throw;
    }
}

void ForwardDisplayProgram::draw(
    CommandList& commands,
    const ResolvedImage& sceneColor,
    const ForwardDisplayUvTransform& uvTransform)
{
    if (!m_pipeline || !m_parameters || m_sampler == VK_NULL_HANDLE) {
        throw std::runtime_error("Forward display program is not initialized");
    }
    if (!sceneColor.complete() || !sceneColor.contentsValid
        || sceneColor.format != m_sceneColorFormat
        || (sceneColor.usage & VK_IMAGE_USAGE_SAMPLED_BIT) == 0
        || sceneColor.currentLayout != VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        throw std::runtime_error(
            "Forward display received an invalid SceneColor binding");
    }

    if (forwardDisplaySourceChanged(
            m_boundGeneration,
            m_boundView,
            sceneColor.allocationGeneration,
            sceneColor.imageView)) {
        m_parameters->writeImage(
            0,
            m_sampler,
            sceneColor.imageView,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        m_boundGeneration = sceneColor.allocationGeneration;
        m_boundView = sceneColor.imageView;
        ++m_descriptorRebindCount;
        KU_INFO(
            "KUENGINE_FORWARD_DISPLAY_BIND generation={} extent={}x{} rebinds={}",
            m_boundGeneration,
            sceneColor.extent.width,
            sceneColor.extent.height,
            m_descriptorRebindCount);
    }

    m_pipeline->bind(commands);
    const VkDescriptorSet parameterSet = m_parameters->set();
    vkCmdBindDescriptorSets(
        commands.cmd(),
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        m_pipeline->layout(),
        0,
        1,
        &parameterSet,
        0,
        nullptr);
    const DisplayPushConstants push{
        uvTransform.scale,
        uvTransform.offset,
    };
    vkCmdPushConstants(
        commands.cmd(),
        m_pipeline->layout(),
        VK_SHADER_STAGE_FRAGMENT_BIT,
        0,
        sizeof(push),
        &push);
    commands.draw(3);
}

} // namespace ku
