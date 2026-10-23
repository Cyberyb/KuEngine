#include "GBufferDisplayPass.h"
#include "ForwardGraphTargets.h"
#include "RenderGraphResources.h"
#include <KuEngine/RHI/RHIDevice.h>
#include <KuEngine/RHI/RHIParameterSet.h>
#include <KuEngine/RHI/RHIPipeline.h>
#include <KuEngine/RHI/RHIShader.h>
#include <KuEngine/RHI/CommandList.h>
#include <KuEngine/Core/Log.h>
#include <filesystem>
#include <stdexcept>
namespace ku {
struct GBufferDisplayPass::State {
    const RHIDevice* device = nullptr;
    VkSampler sampler = VK_NULL_HANDLE;
    std::unique_ptr<RHIParameterSet> images;
    std::unique_ptr<RHIShader> vertex, fragment;
    std::unique_ptr<RHIPipeline> pipeline;
    std::array<uint64_t,4> generations{};
    std::array<VkImageView,4> views{};
    ~State() { pipeline.reset(); images.reset(); if (sampler) vkDestroySampler(device->device(),sampler,nullptr); }
};
GBufferDisplayPass::GBufferDisplayPass(SceneDrawSource& source, VkFormat output, std::string name)
    : m_source(source), m_fixedFormat(output), m_name(std::move(name)) {}
GBufferDisplayPass::~GBufferDisplayPass() = default;
void GBufferDisplayPass::initialize(const RenderContext& c) {
    validateGBufferFormats(c.device,c.depthFormat);
    m_targets=makeGBufferTargets(c.depthFormat,c.clearDepthStencil);
    m_output=runtimeColorImageDesc(c);
    if (m_fixedFormat != VK_FORMAT_UNDEFINED) { m_output.format=m_fixedFormat; m_output.usage=VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT; m_output.initialContent=InitialContent::Cleared; }
    VkFormatProperties properties{}; vkGetPhysicalDeviceFormatProperties(c.device.physicalDevice(),m_output.format,&properties);
    VkFormatFeatureFlags required=VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT;
    if (m_fixedFormat != VK_FORMAT_UNDEFINED) required |= VK_FORMAT_FEATURE_TRANSFER_SRC_BIT;
    if ((properties.optimalTilingFeatures & required) != required) throw std::runtime_error("GBuffer display output format unsupported");
    std::vector<std::unique_ptr<State>> candidates;
    for(uint32_t slot=0;slot<c.framesInFlight;++slot) {
    auto s=std::make_unique<State>(); s->device=&c.device;
    VkSamplerCreateInfo sampler{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    sampler.minFilter=sampler.magFilter=VK_FILTER_NEAREST; sampler.mipmapMode=VK_SAMPLER_MIPMAP_MODE_NEAREST;
    sampler.addressModeU=sampler.addressModeV=sampler.addressModeW=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    VK_CHECK(vkCreateSampler(c.device.device(),&sampler,nullptr,&s->sampler));
    std::vector<ParameterBindingDesc> bindings;
    for(uint32_t i=0;i<4;++i) bindings.push_back({i,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,VK_SHADER_STAGE_FRAGMENT_BIT});
    s->images=std::make_unique<RHIParameterSet>(c.device,bindings);
    const auto directory=std::filesystem::current_path()/"shaders";
    s->vertex=std::make_unique<RHIShader>(c.device,ShaderDesc{directory/"forward/display.vert.spv",VK_SHADER_STAGE_VERTEX_BIT,"main"});
    s->fragment=std::make_unique<RHIShader>(c.device,ShaderDesc{directory/"deferred/debug.frag.spv",VK_SHADER_STAGE_FRAGMENT_BIT,"main"});
    GraphicsPipelineDesc desc{}; desc.shaders={*s->vertex,*s->fragment}; desc.colorFormats={m_output.format};
    desc.descriptorSetLayouts={s->images->layout()}; desc.pushConstantRanges={{VK_SHADER_STAGE_FRAGMENT_BIT,0,32}};
    desc.depthTest=desc.depthWrite=false; desc.cullMode=VK_CULL_MODE_NONE;
    s->pipeline=std::make_unique<RHIPipeline>(c.device,desc); candidates.push_back(std::move(s));
    }
    m_states.swap(candidates);
}
void GBufferDisplayPass::setup(RenderGraphBuilder& b) {
    m_source.declarePrepareDependency(b);
    for(size_t i=0;i<4;++i) { m_images[i]=b.createImage(gbufferNames[i],m_targets.colors[i]); b.useImage(m_images[i],ImageUse::FragmentSampled); }
    const auto output=m_fixedFormat==VK_FORMAT_UNDEFINED ? b.importImage(runtime_resource::swapChainColor,m_output) : b.createImage(m_name,m_output);
    b.colorAttachment(output,AttachmentLoadPolicy::Clear,AttachmentStorePolicy::Store);
}
void GBufferDisplayPass::execute(CommandList& commands,const FrameData& frame,RenderGraphResourceResolver& r) {
    auto& state = *m_states.at(frame.frameIndex);
    const auto view=m_source.pathControl().view();
    if(m_source.pathControl().path()!=asset::RenderPath::Deferred || view==GBufferView::Scene) throw std::runtime_error("GBuffer display requires active Deferred debug view");
    (void)m_source.checkedSceneSnapshot(frame);
    VkExtent2D extent{};
    for(size_t i=0;i<4;++i) {
        const auto image=r.resolveImage(m_images[i],ImageUse::FragmentSampled);
        if(!image.complete() || !image.contentsValid || image.format!=gbufferFormat || image.currentLayout!=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL || !(image.usage&VK_IMAGE_USAGE_SAMPLED_BIT) || (i && (extent.width!=image.extent.width || extent.height!=image.extent.height))) throw std::runtime_error("Invalid GBuffer display input");
        extent=image.extent;
        if(forwardDisplaySourceChanged(state.generations[i],state.views[i],image.allocationGeneration,image.imageView)) {
            state.images->writeImage(static_cast<uint32_t>(i),state.sampler,image.imageView,image.currentLayout);
            state.generations[i]=image.allocationGeneration; state.views[i]=image.imageView;
            KU_INFO("KUENGINE_GBUFFER_DISPLAY_BIND name={} slot={} generation={} extent={}x{}",m_name,i,image.allocationGeneration,extent.width,extent.height);
        }
    }
    const auto uv=calculateForwardDisplayUvTransform(frame.viewerLayout.sceneFramebuffer,extent);
    struct Push {std::array<float,2> scale,offset;std::array<float,4> params;};
    const Push push{uv.scale,uv.offset,{float(static_cast<uint32_t>(view)),needsManualDisplayEncoding(m_output.format,true)?1.f:0.f,0,0}};
    state.pipeline->bind(commands); auto set=state.images->set();
    vkCmdBindDescriptorSets(commands,VK_PIPELINE_BIND_POINT_GRAPHICS,state.pipeline->layout(),0,1,&set,0,nullptr);
    vkCmdPushConstants(commands,state.pipeline->layout(),VK_SHADER_STAGE_FRAGMENT_BIT,0,sizeof(push),&push); commands.draw(3);
}
}
