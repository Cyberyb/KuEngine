#include "ForwardDisplayPass.h"

#include "ForwardGraphTargets.h"
#include "RenderGraphResources.h"
#include "DeferredCommon.h"

#include <stdexcept>

namespace ku {

void ForwardDisplayPass::initialize(const RenderContext& context)
{
    const ForwardGraphTargets targets = makeForwardGraphTargets(context);
    validateForwardGraphTargetFormats(
        context.device, targets.color.format, targets.depth.format);
    m_sceneColorDesc = targets.color;
    if (m_deferredSource) m_sceneColorDesc = makeDeferredSceneColor(context);
    m_swapChainDesc = runtimeColorImageDesc(context);

    auto candidate = std::make_unique<ForwardDisplayProgram>();
    candidate->initialize(
        context.device,
        m_sceneColorDesc.format,
        m_swapChainDesc.format, context.framesInFlight);
    m_program = std::move(candidate);
}

void ForwardDisplayPass::setup(RenderGraphBuilder& builder)
{
    if (!m_prepareDependency.empty()) builder.dependsOn(m_prepareDependency);
    m_sceneColor = builder.createImage(
        m_deferredSource ? deferredSceneColorName : forward_graph_resource::sceneColor,
        m_sceneColorDesc);
    builder.useImage(m_sceneColor, ImageUse::FragmentSampled);
    const ImageHandle swapChainColor = builder.importImage(
        runtime_resource::swapChainColor,
        m_swapChainDesc);
    builder.colorAttachment(
        swapChainColor,
        AttachmentLoadPolicy::Clear,
        AttachmentStorePolicy::Store);
}

void ForwardDisplayPass::execute(
    CommandList& commands,
    const FrameData& frame,
    RenderGraphResourceResolver& resources)
{
    if (!m_program) {
        throw std::runtime_error("Forward display pass is not initialized");
    }
    const ResolvedImage sceneColor = resources.resolveImage(
        m_sceneColor,
        ImageUse::FragmentSampled);
    const ForwardDisplayUvTransform uvTransform =
        calculateForwardDisplayUvTransform(
            frame.viewerLayout.sceneFramebuffer,
            sceneColor.extent);
    const auto view = m_viewProvider ? m_viewProvider(frame) : ForwardView{};
    m_program->draw(commands, sceneColor, uvTransform, view.environmentExposure, view.outputGamma, frame.frameIndex);
}

} // namespace ku
