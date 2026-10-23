#include "VolumetricPass.h"
#include <KuEngine/RHI/CommandList.h>
#include <KuEngine/RHI/RHIDevice.h>
#include <KuEngine/Render/RenderGraph.h>
#include <KuEngine/Render/RenderGraphResources.h>
#include <imgui.h>
#include <filesystem>

namespace ku::volumetric {
namespace {
std::unique_ptr<RHIShader> shader(RHIDevice& device, const char* file, VkShaderStageFlagBits stage)
{
    return std::make_unique<RHIShader>(device, ShaderDesc{
        std::filesystem::current_path()/"shaders"/"volumetric_integration"/file, stage, "main"});
}
GraphicsPipelineDesc pipelineDesc(RHIShader& vert, RHIShader& frag, VkFormat format, uint32_t constants)
{
    GraphicsPipelineDesc desc;
    desc.shaders = {vert, frag};
    desc.colorFormats = {format};
    desc.pushConstantRanges = {{VK_SHADER_STAGE_FRAGMENT_BIT,0,constants}};
    desc.cullMode = VK_CULL_MODE_NONE;
    desc.depthTest = false;
    desc.depthWrite = false;
    return desc;
}
}
ImageDesc sceneColorDesc()
{
    ImageDesc desc;
    desc.extent = ImageExtentDesc::swapchainRelative();
    desc.format = VK_FORMAT_R16G16B16A16_SFLOAT;
    desc.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    desc.aspect = VK_IMAGE_ASPECT_COLOR_BIT;
    desc.initialContent = InitialContent::Undefined;
    return desc;
}
ScenePass::~ScenePass() = default;
void ScenePass::initialize(const RenderContext& context)
{
    m_pipeline.reset();
    m_vert = shader(context.device,"fullscreen.vert.spv",VK_SHADER_STAGE_VERTEX_BIT);
    m_frag = shader(context.device,"volumetric.frag.spv",VK_SHADER_STAGE_FRAGMENT_BIT);
    m_pipeline = std::make_unique<RHIPipeline>(context.device,
        pipelineDesc(*m_vert,*m_frag,sceneColorDesc().format,sizeof(SceneConstants)));
}
void ScenePass::setup(RenderGraphBuilder& builder)
{
    const auto color = builder.createImage(sceneColorName,sceneColorDesc());
    builder.colorAttachment(color,AttachmentLoadPolicy::Clear,AttachmentStorePolicy::Store);
}
void ScenePass::update(const FrameData& frame)
{
    if (!m_settings.paused) m_settings.time += frame.deltaTime;
}
SceneConstants ScenePass::constants(const FrameData& frame) const
{
    const auto& r = frame.viewerLayout.sceneFramebuffer;
    const float time = m_settings.time + m_settings.timeStep * float(frame.submittedFrame - 1);
    return {{float(r.x),float(r.y),float(r.width),float(r.height)},
        {time,m_settings.density,m_settings.light,m_settings.strongFog},
        {m_settings.steps,m_settings.shadowSteps,m_settings.integration,
         (m_settings.noise ? 1 : 0) | (m_settings.shadows ? 2 : 0)}};
}
void ScenePass::execute(CommandList& cmd,const FrameData& frame)
{
    const auto push = constants(frame);
    m_pipeline->bind(cmd);
    vkCmdPushConstants(cmd,m_pipeline->layout(),VK_SHADER_STAGE_FRAGMENT_BIT,0,sizeof(push),&push);
    cmd.draw(3);
}
void ScenePass::drawUI()
{
    ImGui::TextWrapped("VolumetricIntegration by SebH (2015)");
    ImGui::TextWrapped("Shadertoy XlBSRz / CC BY-NC-SA 3.0");
    ImGui::TextWrapped("Original scene/integration; deterministic replacement noise.");
    ImGui::Checkbox("Pause",&m_settings.paused);
    ImGui::SliderFloat("Time (s)",&m_settings.time,0,60,"%.3f");
    ImGui::Combo("Integration",&m_settings.integration,"Analytical (improved)\0Basic\0Transmittance first\0Split: basic | improved\0");
    ImGui::SliderFloat("Fog density",&m_settings.density,0,5);
    ImGui::SliderFloat("Strong height fog",&m_settings.strongFog,0,10);
    ImGui::SliderFloat("Light intensity",&m_settings.light,0,5);
    ImGui::SliderInt("View steps",&m_settings.steps,16,256);
    ImGui::SliderInt("Shadow steps",&m_settings.shadowSteps,1,64);
    ImGui::Checkbox("Fog noise",&m_settings.noise);
    ImGui::Checkbox("Volumetric shadows",&m_settings.shadows);
    if (ImGui::Button("Reset reference defaults")) m_settings = Settings{};
}
DisplayPass::~DisplayPass() { reset(); }
void DisplayPass::reset()
{
    m_pipeline.reset(); m_frag.reset(); m_vert.reset(); m_parameters.clear();
    if (m_sampler && m_device) vkDestroySampler(m_device->device(),m_sampler,nullptr);
    m_sampler = VK_NULL_HANDLE;
}
void DisplayPass::initialize(const RenderContext& context)
{
    reset(); m_device = &context.device;
    m_outputDesc = runtimeColorImageDesc(context);
    m_srgb = context.colorFormat == VK_FORMAT_B8G8R8A8_SRGB || context.colorFormat == VK_FORMAT_R8G8B8A8_SRGB;
    VkSamplerCreateInfo sampler{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    sampler.magFilter = sampler.minFilter = VK_FILTER_NEAREST;
    sampler.addressModeU = sampler.addressModeV = sampler.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    VK_CHECK(vkCreateSampler(context.device.device(),&sampler,nullptr,&m_sampler));
    for (uint32_t slot = 0; slot < context.framesInFlight; ++slot)
        m_parameters.push_back(std::make_unique<RHIParameterSet>(context.device,
            std::vector<ParameterBindingDesc>{{0,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,VK_SHADER_STAGE_FRAGMENT_BIT}}));
    m_vert = shader(context.device,"fullscreen.vert.spv",VK_SHADER_STAGE_VERTEX_BIT);
    m_frag = shader(context.device,"display.frag.spv",VK_SHADER_STAGE_FRAGMENT_BIT);
    auto desc = pipelineDesc(*m_vert,*m_frag,context.colorFormat,sizeof(float)*4);
    desc.descriptorSetLayouts = {m_parameters.front()->layout()};
    m_pipeline = std::make_unique<RHIPipeline>(context.device,desc);
}
void DisplayPass::setup(RenderGraphBuilder& builder)
{
    m_scene = builder.createImage(sceneColorName,sceneColorDesc());
    builder.useImage(m_scene,ImageUse::FragmentSampled);
    const auto output = builder.importImage(runtime_resource::swapChainColor,m_outputDesc);
    builder.colorAttachment(output,AttachmentLoadPolicy::Clear,AttachmentStorePolicy::Store);
}
void DisplayPass::execute(CommandList& cmd,const FrameData& frame,RenderGraphResourceResolver& resources)
{
    const auto image = resources.resolveImage(m_scene,ImageUse::FragmentSampled);
    auto& parameters = *m_parameters.at(frame.frameIndex); // slot fence has completed
    parameters.writeImage(0,m_sampler,image.imageView,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    const auto set = parameters.set();
    m_pipeline->bind(cmd);
    vkCmdBindDescriptorSets(cmd,VK_PIPELINE_BIND_POINT_GRAPHICS,m_pipeline->layout(),0,1,&set,0,nullptr);
    const std::array<float,4> push{float(image.extent.width),float(image.extent.height),m_srgb ? 1.f : 0.f,0};
    vkCmdPushConstants(cmd,m_pipeline->layout(),VK_SHADER_STAGE_FRAGMENT_BIT,0,sizeof(push),push.data());
    cmd.draw(3);
}
} // namespace ku::volumetric
