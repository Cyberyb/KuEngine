#pragma once

#include <memory>
#include <optional>
#include <string_view>
#include <vector>

#include <KuEngine/Asset/Scene.h>
#include <KuEngine/Render/ForwardCommon.h>
#include <KuEngine/Render/RenderPass.h>
#include <KuEngine/Render/DeferredGeometryPass.h>
#include <KuEngine/Render/ScenePasses.h>

namespace ku {
class ForwardProgram;
class ForwardRenderer;
class GpuModelAsset;
class PBRMaterialResources;
using MaterialGpuResources = PBRMaterialResources;
class PBREnvironmentResources;

class ForwardReusePass final : public RenderPass, public SceneDrawSource {
public:
    explicit ForwardReusePass(
        bool zeroLights = false,
        bool backFacePreset = false,
        bool deferred = false,
        int lightPreset = 0, int fixture = 0, float normalScale = 1, float exposure = 1, bool sky = false, bool environment = false,
        std::optional<asset::RenderPath> pathOverride = std::nullopt);
    ~ForwardReusePass() override;
    [[nodiscard]] std::string_view name() const override { return "ForwardReuseScenePrepare"; }
    std::string_view scenePrepareName() const override { return name(); }
    const SceneRenderSnapshot& sceneSnapshot() const override { return m_snapshot; }
    bool sceneSourceEnabled() const override { return enabled(); }
    ForwardRenderer& sceneRenderer(SceneDrawPhase phase) override;
    void setPathSmokeVariant(bool variant) { m_pathSmokeVariant = variant; }
    [[nodiscard]] ForwardView deferredView() const { return m_snapshot.view; }
    void initialize(const RenderContext& context) override;
    void setup(RenderGraphBuilder& builder) override;
    void update(const FrameData&) override;
    void execute(CommandList& commandList, const FrameData& frame) override;
    void drawUI() override;
    [[nodiscard]] std::optional<CommandListStatistics>
    expectedFrameStatistics() const override;

private:
    ImageDesc m_colorTargetDesc{};
    ImageDesc m_depthTargetDesc{};
    bool m_zeroLights = false;
    bool m_pathSmokeVariant = false;
    bool m_deferred = false;
    std::optional<asset::RenderPath> m_pathOverride;
    int m_lightPreset = 0; // 0 scene, 1 directional-only, 2 point-only
    SceneRenderSnapshot m_snapshot{};
    int m_viewPreset = 0;
    int m_fixture = 0;
    float m_normalScale = 1, m_exposure = 1;
    bool m_sky = false, m_environmentEnabled = false;
    asset::SceneData m_scene;
    MaterialGpuVariantPlan m_variantPlan;
    std::unique_ptr<ForwardProgram> m_program;
    std::unique_ptr<ForwardRenderer> m_renderer;
    std::unique_ptr<ForwardProgram> m_geometryProgram;
    std::unique_ptr<ForwardRenderer> m_geometryRenderer;
    std::unique_ptr<ForwardRenderer> m_blendRenderer;
    std::unique_ptr<ForwardRenderer> m_backgroundRenderer;
    std::unique_ptr<PBREnvironmentResources> m_environment;
    std::vector<std::unique_ptr<MaterialGpuResources>> m_materialVariants;
    std::unique_ptr<GpuModelAsset> m_model;
    int m_selectedInstance = 0;
};
} // namespace ku
