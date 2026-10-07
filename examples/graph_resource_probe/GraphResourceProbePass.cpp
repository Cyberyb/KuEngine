#include "GraphResourceProbePass.h"

#include <KuEngine/Core/Log.h>
#include <KuEngine/Core/RuntimeError.h>
#include <KuEngine/RHI/CommandList.h>
#include <KuEngine/RHI/RHIDevice.h>
#include <KuEngine/RHI/RHIPipeline.h>
#include <KuEngine/RHI/RHIShader.h>
#include <KuEngine/Render/RenderContext.h>
#include <KuEngine/Render/RenderGraphResources.h>

#include <array>
#include <bit>
#include <filesystem>
#include <stdexcept>

namespace ku {
namespace {

constexpr VkDeviceSize kProbeBufferSize = 128;
constexpr VkDeviceSize kVertexOffset = 0;
constexpr VkDeviceSize kVertexBytes = sizeof(float) * 6;
constexpr VkDeviceSize kIndexOffset = 32;
constexpr VkDeviceSize kIndexBytes = sizeof(uint32_t) * 3;
constexpr VkDeviceSize kIndirectOffset = 48;
constexpr VkDeviceSize kIndirectBytes = sizeof(VkDrawIndexedIndirectCommand);

ImageDesc probeImageDesc(const RenderContext& context)
{
    ImageDesc desc = runtimeColorImageDesc(context);
    desc.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
        | VK_IMAGE_USAGE_SAMPLED_BIT;
    desc.clearValue.color = {{0.12f, 0.28f, 0.82f, 1.0f}};
    return desc;
}

BufferDesc sourceBufferDesc()
{
    return {
        .size = kProbeBufferSize,
        .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT
            | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        .memoryUsage = VMA_MEMORY_USAGE_AUTO,
        .initialContent = InitialContent::Undefined,
    };
}

BufferDesc drawBufferDesc()
{
    return {
        .size = kProbeBufferSize,
        .usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT
            | VK_BUFFER_USAGE_TRANSFER_SRC_BIT
            | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
            | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT
            | VK_BUFFER_USAGE_INDEX_BUFFER_BIT
            | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,
        .memoryUsage = VMA_MEMORY_USAGE_AUTO,
        .initialContent = InitialContent::Undefined,
    };
}

BufferDesc computeBufferDesc()
{
    return {
        .size = kProbeBufferSize,
        .usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT
            | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        .memoryUsage = VMA_MEMORY_USAGE_AUTO,
        .initialContent = InitialContent::Undefined,
    };
}

BufferDesc resultBufferDesc()
{
    return {
        .size = kProbeBufferSize,
        .usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        .memoryUsage = VMA_MEMORY_USAGE_AUTO,
        .initialContent = InitialContent::Undefined,
    };
}

void fillWord(CommandList& cmd, VkBuffer buffer, VkDeviceSize offset, uint32_t word)
{
    cmd.fillBuffer(buffer, offset, sizeof(uint32_t), word);
}

} // namespace

void GraphResourceImageClearPass::initialize(const RenderContext& context)
{
    m_imageDesc = probeImageDesc(context);
}

void GraphResourceImageClearPass::setup(RenderGraphBuilder& builder)
{
    m_image = builder.createImage("ProbeInternalColor", m_imageDesc);
    builder.colorAttachment(
        m_image,
        AttachmentLoadPolicy::Clear,
        AttachmentStorePolicy::Store);
}

void GraphResourceImageClearPass::execute(
    CommandList&,
    const FrameData&,
    RenderGraphResourceResolver& resources)
{
    const ResolvedImage image = resources.resolveImage(
        m_image, ImageUse::ColorAttachment);
    if (!image.complete() || image.external) {
        throw std::runtime_error(
            "GraphResourceProbe failed to resolve internal image");
    }
}

void GraphResourceClearPass::initialize(const RenderContext&)
{
    m_sourceDesc = sourceBufferDesc();
}

void GraphResourceClearPass::setup(RenderGraphBuilder& builder)
{
    m_source = builder.createBuffer("ProbeSourceBuffer", m_sourceDesc);
    builder.useBuffer(m_source, BufferUse::TransferDestination);
}

void GraphResourceClearPass::execute(
    CommandList& cmd,
    const FrameData&,
    RenderGraphResourceResolver& resources)
{
    const ResolvedBuffer source = resources.resolveBuffer(
        m_source, BufferUse::TransferDestination);
    if (!source.complete() || source.external) {
        throw std::runtime_error(
            "GraphResourceProbe failed to resolve internal allocations");
    }

    constexpr float positions[] = {
        -0.72f, -0.62f,
         0.72f, -0.62f,
         0.00f,  0.72f,
    };
    for (size_t index = 0; index < std::size(positions); ++index) {
        fillWord(cmd, source.buffer,
            kVertexOffset + index * sizeof(uint32_t),
            std::bit_cast<uint32_t>(positions[index]));
    }
    constexpr uint32_t indices[] = {0, 1, 2};
    for (size_t index = 0; index < std::size(indices); ++index) {
        fillWord(cmd, source.buffer,
            kIndexOffset + index * sizeof(uint32_t), indices[index]);
    }
    const VkDrawIndexedIndirectCommand indirect{3, 1, 0, 0, 0};
    const auto words = std::bit_cast<std::array<uint32_t, 5>>(indirect);
    for (size_t index = 0; index < words.size(); ++index) {
        fillWord(cmd, source.buffer,
            kIndirectOffset + index * sizeof(uint32_t), words[index]);
    }
}

CallbackPassDesc<GraphResourceCopyParameters>
makeGraphResourceCopyCallback()
{
    CallbackPassDesc<GraphResourceCopyParameters> desc{};
    desc.name = "GraphResourceCopyCallback";
    desc.nativeContract = {
        NativeCommandCapability::Transfer,
        NativeCommandSideEffect::ReadsDeclaredResources
            | NativeCommandSideEffect::WritesDeclaredResources};
    desc.declare = [](RenderGraphBuilder& builder, auto& parameters) {
        parameters.m_sourceDesc = sourceBufferDesc();
        parameters.m_computeDesc = computeBufferDesc();
        parameters.m_source = builder.createBuffer(
            "ProbeSourceBuffer", parameters.m_sourceDesc);
        parameters.m_compute = builder.createBuffer(
            "ProbeComputeInput", parameters.m_computeDesc);
        builder.useBuffer(parameters.m_source, BufferUse::TransferSource);
        builder.useBuffer(parameters.m_compute, BufferUse::TransferDestination);
    };
    desc.execute = [](GraphCommandContext& context, const FrameData&, const auto& p) {
        auto native = context.nativeCommands({
            NativeCommandCapability::Transfer,
            NativeCommandSideEffect::ReadsDeclaredResources
                | NativeCommandSideEffect::WritesDeclaredResources});
        const ResolvedBuffer source = native.resolveBuffer(
            p.m_source, BufferUse::TransferSource, {0, kProbeBufferSize});
        const ResolvedBuffer compute = native.resolveBuffer(
            p.m_compute, BufferUse::TransferDestination, {0, kProbeBufferSize});
        const VkBufferCopy copy{0, 0, kProbeBufferSize};
        vkCmdCopyBuffer(
            native.commandBuffer(), source.buffer, compute.buffer, 1, &copy);
    };
    desc.expectedStatistics = CommandListStatistics{0, 0};
    return desc;
}

CallbackPassDesc<GraphResourceComputeParameters>
makeGraphResourceComputeCallback()
{
    CallbackPassDesc<GraphResourceComputeParameters> desc{};
    desc.name = "GraphResourceComputeCallback";
    desc.declare = [](RenderGraphBuilder& builder, auto& parameters) {
        parameters.m_computeDesc = computeBufferDesc();
        parameters.m_drawDesc = drawBufferDesc();
        parameters.m_compute = builder.createBuffer(
            "ProbeComputeInput", parameters.m_computeDesc);
        parameters.m_draw = builder.createBuffer(
            "ProbeDrawBuffer", parameters.m_drawDesc);
        builder.useBuffer(parameters.m_compute, BufferUse::ComputeStorageRead);
        builder.useBuffer(parameters.m_draw, BufferUse::ComputeStorageWrite);
    };
    desc.prepare = [](const RenderContext& context, auto& parameters) {
        if (!context.device.graphicsQueueSupportsCompute()) {
            throw RuntimeUnavailableError(
                "GraphResourceProbe requires compute support on the graphics queue");
        }
        parameters.m_pipeline.reset();
        parameters.m_parameters.reset();
        parameters.m_shader.reset();
        const auto shaderPath = std::filesystem::current_path()
            / "shaders" / "graph_resource_probe.comp.spv";
        parameters.m_shader = std::make_unique<RHIShader>(
            context.device,
            ShaderDesc{shaderPath, VK_SHADER_STAGE_COMPUTE_BIT, "main"});
        parameters.m_parameters = std::make_unique<RHIParameterSet>(
            context.device,
            std::vector<ParameterBindingDesc>{
                {0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                    VK_SHADER_STAGE_COMPUTE_BIT},
                {1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                    VK_SHADER_STAGE_COMPUTE_BIT}});
        ComputePipelineDesc pipelineDesc{};
        pipelineDesc.shader = parameters.m_shader.get();
        pipelineDesc.descriptorSetLayouts = {parameters.m_parameters->layout()};
        parameters.m_pipeline = std::make_unique<RHIComputePipeline>(
            context.device, pipelineDesc);
        KU_INFO("KUENGINE_GRAPH_COMPUTE_READY local_size=32 groups=1");
    };
    desc.execute = [](GraphCommandContext& context, const FrameData&, const auto& p) {
        const ResolvedBuffer input = context.resolveBuffer(
            p.m_compute, BufferUse::ComputeStorageRead, {0, kProbeBufferSize});
        const ResolvedBuffer draw = context.resolveBuffer(
            p.m_draw, BufferUse::ComputeStorageWrite, {0, kProbeBufferSize});
        p.m_parameters->writeBuffer(
            0, input.buffer, input.size, 0, kProbeBufferSize);
        p.m_parameters->writeBuffer(
            1, draw.buffer, draw.size, 0, kProbeBufferSize);
        context.bindComputePipeline(*p.m_pipeline);
        context.bindParameterSet(*p.m_pipeline, 0, *p.m_parameters);
        context.dispatch(1, 1, 1);
    };
    desc.expectedStatistics = CommandListStatistics{0, 0};
    return desc;
}

GraphResourceDrawPass::GraphResourceDrawPass() = default;

GraphResourceDrawPass::~GraphResourceDrawPass()
{
    resetGpuResources();
}

void GraphResourceDrawPass::resetGpuResources() noexcept
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
    m_boundImageGeneration = 0;
}

void GraphResourceDrawPass::initialize(const RenderContext& context)
{
    const ImageDesc candidateImageDesc = probeImageDesc(context);
    const ImageDesc candidateSwapchainDesc = runtimeColorImageDesc(context);
    const BufferDesc candidateDrawDesc = drawBufferDesc();
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_NEAREST;
    samplerInfo.minFilter = VK_FILTER_NEAREST;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.maxLod = 0.0f;
    VkSampler candidateSampler = VK_NULL_HANDLE;
    VK_CHECK(vkCreateSampler(
        context.device.device(), &samplerInfo, nullptr, &candidateSampler));

    try {
        auto candidateParameters = std::make_unique<RHIParameterSet>(
            context.device,
            std::vector<ParameterBindingDesc>{
                {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                    VK_SHADER_STAGE_FRAGMENT_BIT}});
        const auto shaderDir = std::filesystem::current_path() / "shaders";
        auto candidateVertexShader = std::make_unique<RHIShader>(
            context.device,
            ShaderDesc{shaderDir / "graph_resource_probe.vert.spv",
                VK_SHADER_STAGE_VERTEX_BIT, "main"});
        auto candidateFragmentShader = std::make_unique<RHIShader>(
            context.device,
            ShaderDesc{shaderDir / "graph_resource_probe.frag.spv",
                VK_SHADER_STAGE_FRAGMENT_BIT, "main"});
        GraphicsPipelineDesc pipelineDesc{};
        pipelineDesc.shaders = {
            *candidateVertexShader, *candidateFragmentShader};
        pipelineDesc.colorFormats = {context.colorFormat};
        pipelineDesc.vertexBindings = {
            {0, sizeof(float) * 2, VK_VERTEX_INPUT_RATE_VERTEX}};
        pipelineDesc.vertexAttributes = {
            {0, 0, VK_FORMAT_R32G32_SFLOAT, 0}};
        pipelineDesc.descriptorSetLayouts = {candidateParameters->layout()};
        pipelineDesc.depthFormat = VK_FORMAT_UNDEFINED;
        pipelineDesc.cullMode = VK_CULL_MODE_NONE;
        pipelineDesc.depthTest = false;
        pipelineDesc.depthWrite = false;
        auto candidatePipeline = std::make_unique<RHIPipeline>(
            context.device, pipelineDesc);

        resetGpuResources();
        m_device = &context.device;
        m_imageDesc = candidateImageDesc;
        m_swapchainDesc = candidateSwapchainDesc;
        m_drawDesc = candidateDrawDesc;
        m_sampler = candidateSampler;
        candidateSampler = VK_NULL_HANDLE;
        m_parameters = std::move(candidateParameters);
        m_vertexShader = std::move(candidateVertexShader);
        m_fragmentShader = std::move(candidateFragmentShader);
        m_pipeline = std::move(candidatePipeline);
        m_initialImageGeneration = 0;
        m_initialBufferGeneration = 0;
        m_initialExtent = {0, 0};
        m_resizeReported = false;
    } catch (...) {
        if (candidateSampler != VK_NULL_HANDLE) {
            vkDestroySampler(
                context.device.device(), candidateSampler, nullptr);
        }
        throw;
    }
}

void GraphResourceDrawPass::setup(RenderGraphBuilder& builder)
{
    m_image = builder.createImage("ProbeInternalColor", m_imageDesc);
    m_draw = builder.createBuffer("ProbeDrawBuffer", m_drawDesc);
    builder.useImage(m_image, ImageUse::FragmentSampled);
    builder.useBuffer(m_draw, BufferUse::Vertex);
    builder.useBuffer(m_draw, BufferUse::Index);
    builder.useBuffer(m_draw, BufferUse::Indirect);
    builder.colorAttachment(
        builder.importImage(runtime_resource::swapChainColor, m_swapchainDesc),
        AttachmentLoadPolicy::Clear,
        AttachmentStorePolicy::Store);
    builder.exportImage(m_image);
}

void GraphResourceDrawPass::execute(
    CommandList& cmd,
    const FrameData&,
    RenderGraphResourceResolver& resources)
{
    const ResolvedImage image = resources.resolveImage(
        m_image, ImageUse::FragmentSampled);
    const ResolvedBuffer vertex = resources.resolveBuffer(
        m_draw, BufferUse::Vertex);
    const ResolvedBuffer index = resources.resolveBuffer(
        m_draw, BufferUse::Index);
    const ResolvedBuffer indirect = resources.resolveBuffer(
        m_draw, BufferUse::Indirect);
    if (!image.contentsValid || !vertex.contentsValid) {
        throw std::runtime_error(
            "GraphResourceProbe draw pass received invalid contents");
    }

    if (image.allocationGeneration != m_boundImageGeneration) {
        m_parameters->writeImage(
            0,
            m_sampler,
            image.imageView,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        m_boundImageGeneration = image.allocationGeneration;
    }

    m_pipeline->bind(cmd);
    const VkDescriptorSet descriptorSet = m_parameters->set();
    vkCmdBindDescriptorSets(
        cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline->layout(),
        0, 1, &descriptorSet, 0, nullptr);
    cmd.bindVertexBuffer(vertex.buffer, kVertexOffset);
    cmd.bindIndexBuffer(index.buffer, kIndexOffset, VK_INDEX_TYPE_UINT32);
    cmd.drawIndexedIndirect(
        indirect.buffer, kIndirectOffset, 1,
        sizeof(VkDrawIndexedIndirectCommand), 3);

    if (m_initialImageGeneration == 0) {
        m_initialImageGeneration = image.allocationGeneration;
        m_initialBufferGeneration = vertex.allocationGeneration;
        m_initialExtent = image.extent;
        KU_INFO(
            "KUENGINE_GRAPH_RESOURCE_PROBE_READY image_generation={} buffer_generation={} extent={}x{}",
            image.allocationGeneration, vertex.allocationGeneration,
            image.extent.width, image.extent.height);
        return;
    }
    const bool extentChanged = image.extent.width != m_initialExtent.width
        || image.extent.height != m_initialExtent.height;
    if (extentChanged && !m_resizeReported) {
        if (image.allocationGeneration == m_initialImageGeneration
            || vertex.allocationGeneration != m_initialBufferGeneration) {
            throw std::runtime_error(
                "GraphResourceProbe resize allocation generations are incorrect");
        }
        m_resizeReported = true;
        KU_INFO(
            "KUENGINE_GRAPH_RESOURCE_PROBE_RESIZE_OK image_generation={} buffer_generation={} extent={}x{}",
            image.allocationGeneration, vertex.allocationGeneration,
            image.extent.width, image.extent.height);
    }
}

void GraphResourcePostDrawCopyPass::initialize(const RenderContext&)
{
    m_drawDesc = drawBufferDesc();
    m_resultDesc = resultBufferDesc();
}

void GraphResourcePostDrawCopyPass::setup(RenderGraphBuilder& builder)
{
    m_draw = builder.createBuffer("ProbeDrawBuffer", m_drawDesc);
    m_result = builder.createBuffer("ProbePostDrawCopyBuffer", m_resultDesc);
    builder.useBuffer(m_draw, BufferUse::TransferSource);
    builder.useBuffer(m_result, BufferUse::TransferDestination);
    builder.dependsOn("GraphResourceSampleDraw");
}

void GraphResourcePostDrawCopyPass::execute(
    CommandList& cmd,
    const FrameData&,
    RenderGraphResourceResolver& resources)
{
    const ResolvedBuffer draw = resources.resolveBuffer(
        m_draw, BufferUse::TransferSource);
    const ResolvedBuffer result = resources.resolveBuffer(
        m_result, BufferUse::TransferDestination);
    if (!draw.contentsValid || !draw.complete() || !result.complete()) {
        throw std::runtime_error(
            "GraphResourceProbe post-draw copy received invalid resources");
    }
    cmd.copyBuffer(draw.buffer, result.buffer, 0, 0, kProbeBufferSize);
}

} // namespace ku
