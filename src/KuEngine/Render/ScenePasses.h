#pragma once
#include "DeferredGeometryPass.h"
#include "ForwardCommon.h"
#include "ScenePathControl.h"
#include <vector>

namespace ku {
class ForwardRenderer;
enum class SceneDrawPhase { Background, Opaque, Blend, Geometry };
struct SceneRenderSnapshot {
    ForwardView view{};
    std::vector<ForwardDraw> draws;
    float preparedTime = -1.0f;
};
// A CPU scene coordinator prepares this snapshot once after UI, and owns assets
// and independent phase UBOs. Graph passes only borrow it for this submission.
class SceneDrawSource {
public:
    [[nodiscard]] ScenePathControl& pathControl() noexcept { return m_pathControl; }
    [[nodiscard]] const ScenePathControl& pathControl() const noexcept { return m_pathControl; }
    virtual ~SceneDrawSource() = default;
    [[nodiscard]] virtual std::string_view scenePrepareName() const = 0;
    [[nodiscard]] virtual const SceneRenderSnapshot& sceneSnapshot() const = 0;
    [[nodiscard]] virtual bool sceneSourceEnabled() const = 0;
    [[nodiscard]] const SceneRenderSnapshot& checkedSceneSnapshot(const FrameData& frame) const;
    void declarePrepareDependency(RenderGraphBuilder& builder) const;
    [[nodiscard]] virtual ForwardRenderer& sceneRenderer(SceneDrawPhase phase) = 0;
private:
    ScenePathControl m_pathControl;
};
class SceneBackgroundPass final : public RenderPass {
public:
    explicit SceneBackgroundPass(SceneDrawSource& source) : m_source(source) {}
    std::string_view name() const override { return "SceneBackground"; }
    void initialize(const RenderContext&) override;
    void setup(RenderGraphBuilder&) override;
    void execute(CommandList&, const FrameData&) override;
    std::optional<CommandListStatistics> expectedFrameStatistics() const override;
private:
    SceneDrawSource& m_source;
    ImageDesc m_color{};
};
class SceneSurfacePass final : public DeferredGeometryPass {
public:
    SceneSurfacePass(SceneDrawSource& source, SceneDrawPhase phase, bool deferred = false);
    std::string_view name() const override;
    void initialize(const RenderContext&) override;
    void setup(RenderGraphBuilder&) override;
    void execute(CommandList&, const FrameData&) override;
    std::optional<CommandListStatistics> expectedFrameStatistics() const override;
private:
    SceneDrawSource& m_source;
    SceneDrawPhase m_phase;
    bool m_deferred;
    ImageDesc m_color{}, m_depth{};
};
} // namespace ku
