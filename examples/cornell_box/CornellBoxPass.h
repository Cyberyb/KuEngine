#pragma once

#include "CornellBoxScene.h"
#include <KuEngine/Render/ScenePasses.h>

#include <array>
#include <memory>

namespace ku {
class ForwardProgram;
class ForwardRenderer;
class GpuModelAsset;
class PBRMaterialResources;
class PBREnvironmentResources;

class CornellBoxPass final : public RenderPass, public SceneDrawSource {
public:
    explicit CornellBoxPass(asset::RenderPath path = asset::RenderPath::Deferred);
    ~CornellBoxPass() override;
    std::string_view name() const override { return "CornellBoxScenePrepare"; }
    std::string_view scenePrepareName() const override { return name(); }
    const SceneRenderSnapshot& sceneSnapshot() const override { return m_snapshot; }
    bool sceneSourceEnabled() const override { return enabled(); }
    ForwardRenderer& sceneRenderer(SceneDrawPhase) override;
    // Optional bounded GPU verification; no UI or Runtime state machine.
    void verifyFrameIsolation(bool enabled) { m_verifyFrameIsolation = enabled; }
    void initialize(const RenderContext&) override;
    void update(const FrameData&) override;
    void execute(CommandList&,const FrameData&) override;
    void drawUI() override;
    std::optional<CommandListStatistics> expectedFrameStatistics() const override { return CommandListStatistics{0,0}; }
private:
    asset::RenderPath m_initialPath;
    asset::MeshData m_mesh;
    SceneRenderSnapshot m_snapshot;
    std::unique_ptr<ForwardProgram> m_program, m_geometryProgram;
    std::array<std::unique_ptr<ForwardRenderer>,4> m_renderers;
    std::unique_ptr<GpuModelAsset> m_model;
    std::unique_ptr<PBRMaterialResources> m_materials;
    std::unique_ptr<PBREnvironmentResources> m_environment;
    float m_lightStrength = 1, m_exposure = 1, m_fill = .25f;
    float m_cameraYaw = 0, m_cameraPitch = 0, m_cameraDistance = 4.1f;
    int m_selectedBox = 0;
    bool m_verifyFrameIsolation = false;
};
} // namespace ku
