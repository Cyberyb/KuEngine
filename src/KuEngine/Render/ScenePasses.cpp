#include "ScenePasses.h"
#include "ForwardGraphTargets.h"
#include "ForwardRenderer.h"
#include "GpuModelAsset.h"
#include "GpuMesh.h"
#include <KuEngine/RHI/CommandList.h>
#include <stdexcept>
namespace ku {
void SceneDrawSource::declarePrepareDependency(RenderGraphBuilder& builder) const { builder.dependsOn(scenePrepareName()); }
const SceneRenderSnapshot& SceneDrawSource::checkedSceneSnapshot(const FrameData& frame) const {
    const auto& snapshot = sceneSnapshot();
    if (!sceneSourceEnabled() || snapshot.preparedTime != frame.totalTime)
        throw std::runtime_error("ScenePrepare is disabled or its snapshot is stale: " + std::string(scenePrepareName()));
    return snapshot;
}
void SceneBackgroundPass::initialize(const RenderContext& context) {
    const auto targets = makeForwardGraphTargets(context);
    validateForwardGraphTargetFormats(context.device, targets.color.format, targets.depth.format);
    m_color = targets.color;
}
void SceneBackgroundPass::setup(RenderGraphBuilder& b) {
    m_source.declarePrepareDependency(b);
    b.colorAttachment(b.createImage(forward_graph_resource::sceneColor, m_color),
        AttachmentLoadPolicy::Clear, AttachmentStorePolicy::Store);
}
void SceneBackgroundPass::execute(CommandList& commands, const FrameData& frame) {
    std::string error;
    if (!m_source.sceneRenderer(SceneDrawPhase::Background).render(commands,
            m_source.checkedSceneSnapshot(frame).view, {}, error, frame.frameIndex))
        throw std::runtime_error("Scene background: " + error);
}
std::optional<CommandListStatistics> SceneBackgroundPass::expectedFrameStatistics() const {
    return m_source.sceneSnapshot().view.skyboxEnabled ? CommandListStatistics{1,3} : CommandListStatistics{0,0};
}
SceneSurfacePass::SceneSurfacePass(SceneDrawSource& source, SceneDrawPhase phase, bool deferred)
    : m_source(source), m_phase(phase), m_deferred(deferred) {
    if (phase == SceneDrawPhase::Background || (deferred && phase == SceneDrawPhase::Blend))
        throw std::invalid_argument("Invalid scene surface phase");
}
std::string_view SceneSurfacePass::name() const {
    return m_phase == SceneDrawPhase::Blend ? "ForwardBlend" : m_deferred ? "DeferredGeometry" : "ForwardOpaque";
}
void SceneSurfacePass::initialize(const RenderContext& context) {
    const auto targets = makeForwardGraphTargets(context);
    m_color = targets.color; m_depth = targets.depth;
    if (m_deferred) DeferredGeometryPass::initialize(context);
}
void SceneSurfacePass::setup(RenderGraphBuilder& b) {
    m_source.declarePrepareDependency(b);
    if (m_deferred) { DeferredGeometryPass::setup(b); return; }
    b.colorAttachment(b.createImage(forward_graph_resource::sceneColor, m_color),
        AttachmentLoadPolicy::Load, AttachmentStorePolicy::Store);
    b.depthAttachment(b.createImage(forward_graph_resource::sceneDepth, m_depth),
        m_phase == SceneDrawPhase::Blend ? AttachmentLoadPolicy::Load : AttachmentLoadPolicy::Clear,
        AttachmentStorePolicy::Store);
}
void SceneSurfacePass::execute(CommandList& commands, const FrameData& frame) {
    const auto& snapshot = m_source.checkedSceneSnapshot(frame);
    auto view = snapshot.view; view.skyboxEnabled = false;
    std::vector<ForwardDraw> draws;
    for (const auto& draw : snapshot.draws)
        if ((draw.material.alphaMode == asset::AlphaMode::Blend) == (m_phase == SceneDrawPhase::Blend))
            draws.push_back(draw);
    std::string error;
    if (!m_source.sceneRenderer(m_deferred ? SceneDrawPhase::Geometry : m_phase).render(commands, view, draws, error, frame.frameIndex))
        throw std::runtime_error("Scene surfaces: " + error);
}
std::optional<CommandListStatistics> SceneSurfacePass::expectedFrameStatistics() const {
    CommandStatisticsAccumulator stats;
    for (const auto& draw : m_source.sceneSnapshot().draws) {
        if ((draw.material.alphaMode == asset::AlphaMode::Blend) != (m_phase == SceneDrawPhase::Blend)) continue;
        if (draw.model && draw.subMeshIndex < draw.model->mesh().subMeshes().size()
            && draw.model->mesh().subMeshes()[draw.subMeshIndex].indexCount > 0)
            stats.recordIndexedDraw(draw.model->mesh().subMeshes()[draw.subMeshIndex].indexCount, 1);
    }
    return stats.statistics();
}
} // namespace ku
