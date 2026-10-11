#include "DeferredGeometryPass.h"
#include "RenderGraph.h"
#include "ForwardGraphTargets.h"
namespace ku {
void DeferredGeometryPass::initialize(const RenderContext& context)
{
    validateGBufferFormats(context.device, context.depthFormat);
    m_gbufferTargets = makeGBufferTargets(context.depthFormat, context.clearDepthStencil);
}
void DeferredGeometryPass::setup(RenderGraphBuilder& builder)
{
    for (size_t i = 0; i < gbufferAttachmentCount; ++i)
        builder.colorAttachment(builder.createImage(gbufferNames[i], m_gbufferTargets.colors[i]),
            AttachmentLoadPolicy::Clear, AttachmentStorePolicy::Store);
    builder.depthAttachment(builder.createImage(forward_graph_resource::sceneDepth, m_gbufferTargets.depth),
        AttachmentLoadPolicy::Clear, AttachmentStorePolicy::Store);
}
} // namespace ku
