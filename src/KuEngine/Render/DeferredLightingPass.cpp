#include "DeferredLightingPass.h"
#include "ForwardGraphTargets.h"
#include "RenderGraphResources.h"
#include "PBRResources.h"
#include <KuEngine/Core/Log.h>
#include <KuEngine/RHI/RHIBuffer.h>
#include <KuEngine/RHI/RHIDevice.h>
#include <KuEngine/RHI/RHIParameterSet.h>
#include <KuEngine/RHI/RHIPipeline.h>
#include <KuEngine/RHI/RHIShader.h>
#include <KuEngine/RHI/CommandList.h>
#include <cstring>
#include <filesystem>
#include <stdexcept>

namespace ku {
struct DeferredLightingPass::State {
    const RHIDevice* device = nullptr;
    VkSampler sampler = VK_NULL_HANDLE;
    std::unique_ptr<RHIParameterSet> images, frame, environment;
    std::unique_ptr<RHIBuffer> uniforms;
    std::unique_ptr<RHIShader> vertex, fragment;
    std::unique_ptr<RHIPipeline> pipeline;
    std::array<uint64_t, gbufferAttachmentCount> generations{};
    std::array<VkImageView, gbufferAttachmentCount> views{};
    uint64_t rebinds = 0;
    ~State() {
        pipeline.reset();
        images.reset(); frame.reset(); environment.reset();
        if (sampler != VK_NULL_HANDLE) vkDestroySampler(device->device(), sampler, nullptr);
    }
};
DeferredLightingPass::DeferredLightingPass(ViewProvider provider, std::string prepareDependency)
    : m_viewProvider(std::move(provider)), m_prepareDependency(std::move(prepareDependency))
{
    if (!m_viewProvider) throw std::invalid_argument("Deferred Lighting requires a view provider");
}
DeferredLightingPass::~DeferredLightingPass() = default;
void DeferredLightingPass::initialize(const RenderContext& context)
{
    validateGBufferFormats(context.device, context.depthFormat);
    const auto targets = makeGBufferTargets(context.depthFormat, context.clearDepthStencil);
    const auto output = makeDeferredSceneColor(context);
    validateForwardGraphTargetFormats(context.device, output.format, context.depthFormat);
    std::vector<std::unique_ptr<State>> candidates;
    for (uint32_t slot=0;slot<context.framesInFlight;++slot) {
    auto candidate = std::make_unique<State>();
    candidate->device = &context.device;
    VkSamplerCreateInfo sampler{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    sampler.minFilter = sampler.magFilter = VK_FILTER_NEAREST;
    sampler.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    sampler.addressModeU = sampler.addressModeV = sampler.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    VK_CHECK(vkCreateSampler(context.device.device(), &sampler, nullptr, &candidate->sampler));
    std::vector<ParameterBindingDesc> bindings;
    for (uint32_t i = 0; i < gbufferAttachmentCount; ++i)
        bindings.push_back({i, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT});
    candidate->images = std::make_unique<RHIParameterSet>(context.device, bindings);
    candidate->frame = std::make_unique<RHIParameterSet>(context.device,
        std::vector<ParameterBindingDesc>{{0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT}});
    candidate->environment = std::make_unique<RHIParameterSet>(context.device,
        std::vector<ParameterBindingDesc>{{0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT}});
    RHIBuffer::CreateInfo info{};
    info.size = sizeof(ForwardFrameUniforms);
    info.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    info.allocationFlags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
    candidate->uniforms = std::make_unique<RHIBuffer>(context.device, info);
    candidate->frame->writeBuffer(0, candidate->uniforms->buffer(), info.size, 0, info.size);
    const auto directory = std::filesystem::current_path() / "shaders";
    candidate->vertex = std::make_unique<RHIShader>(context.device,
        ShaderDesc{directory / "forward/display.vert.spv", VK_SHADER_STAGE_VERTEX_BIT, "main"});
    candidate->fragment = std::make_unique<RHIShader>(context.device,
        ShaderDesc{directory / "deferred/lighting.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT, "main"});
    GraphicsPipelineDesc desc{};
    desc.shaders = {*candidate->vertex, *candidate->fragment};
    desc.colorFormats = {output.format};
    desc.descriptorSetLayouts = {candidate->images->layout(), candidate->frame->layout(), candidate->environment->layout()};
    desc.depthTest = desc.depthWrite = false;
    desc.cullMode = VK_CULL_MODE_NONE;
    candidate->pipeline = std::make_unique<RHIPipeline>(context.device, desc);
    candidates.push_back(std::move(candidate));
    }
    m_states.swap(candidates); // only publish complete slot candidates
    m_targets = targets;
    m_outputDesc = output;
}
void DeferredLightingPass::setup(RenderGraphBuilder& builder)
{
    if (!m_prepareDependency.empty()) builder.dependsOn(m_prepareDependency);
    for (size_t i = 0; i < gbufferAttachmentCount; ++i) {
        m_images[i] = builder.createImage(gbufferNames[i], m_targets.colors[i]);
        builder.useImage(m_images[i], ImageUse::FragmentSampled);
    }
    builder.colorAttachment(builder.createImage(deferredSceneColorName, m_outputDesc),
        AttachmentLoadPolicy::Load, AttachmentStorePolicy::Store);
}
void DeferredLightingPass::execute(CommandList& commands, const FrameData& frame,
    RenderGraphResourceResolver& resources)
{
    if (m_states.empty()) throw std::runtime_error("Deferred Lighting is not initialized");
    auto& s = *m_states.at(frame.frameIndex);
    VkExtent2D extent{};
    bool rebound = false;
    for (size_t i = 0; i < gbufferAttachmentCount; ++i) {
        const auto image = resources.resolveImage(m_images[i], ImageUse::FragmentSampled);
        if (!image.complete() || !image.contentsValid || image.format != gbufferFormat
            || image.currentLayout != VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
            || !(image.usage & VK_IMAGE_USAGE_SAMPLED_BIT)
            || (i && (image.extent.width != extent.width || image.extent.height != extent.height)))
            throw std::runtime_error("Deferred Lighting received inconsistent GBuffer resources");
        extent = image.extent;
        if (forwardDisplaySourceChanged(s.generations[i], s.views[i], image.allocationGeneration, image.imageView)) {
            s.images->writeImage(static_cast<uint32_t>(i), s.sampler, image.imageView, image.currentLayout);
            s.generations[i] = image.allocationGeneration;
            s.views[i] = image.imageView;
            rebound = true;
        }
    }
    if (rebound) {
        ++s.rebinds;
        KU_INFO("KUENGINE_DEFERRED_BIND mrt=4 generations={},{},{},{} extent={}x{} rebinds={}",
            s.generations[0], s.generations[1], s.generations[2], s.generations[3], extent.width, extent.height, s.rebinds);
    }
    const auto view = m_viewProvider(frame);
    if (!view.environment || !view.environment->ready())
        throw std::runtime_error("Deferred Lighting requires a ready shared environment descriptor");
    const auto uniforms = makeForwardFrameUniforms(view);
    void* mapped = s.uniforms->map();
    if (!mapped) throw std::runtime_error("Deferred frame UBO mapping failed");
    std::memcpy(mapped, &uniforms, sizeof(uniforms));
    s.uniforms->flush(); s.uniforms->unmap();
    s.pipeline->bind(commands);
    const std::array<VkDescriptorSet, 3> sets{s.images->set(), s.frame->set(), view.environment->descriptorSet()};
    vkCmdBindDescriptorSets(commands, VK_PIPELINE_BIND_POINT_GRAPHICS, s.pipeline->layout(),
        0, static_cast<uint32_t>(sets.size()), sets.data(), 0, nullptr);
    commands.draw(3);
}
} // namespace ku
