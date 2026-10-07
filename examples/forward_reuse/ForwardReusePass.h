#pragma once

#include <memory>
#include <optional>
#include <string_view>
#include <vector>

#include <KuEngine/Asset/Scene.h>
#include <KuEngine/Render/ForwardCommon.h>
#include <KuEngine/Render/RenderPass.h>

namespace ku {
class ForwardProgram;
class ForwardRenderer;
class GpuModelAsset;
class PBRMaterialResources;
using MaterialGpuResources = PBRMaterialResources;
class PBREnvironmentResources;

class ForwardReusePass final : public RenderPass {
public:
    explicit ForwardReusePass(
        bool zeroLights = false,
        bool backFacePreset = false);
    ~ForwardReusePass() override;
    [[nodiscard]] std::string_view name() const override { return "ForwardReuse"; }
    void initialize(const RenderContext& context) override;
    void setup(RenderGraphBuilder& builder) override;
    void execute(CommandList& commandList, const FrameData& frame) override;
    void drawUI() override;
    [[nodiscard]] std::optional<CommandListStatistics>
    expectedFrameStatistics() const override;

private:
    ImageDesc m_colorTargetDesc{};
    ImageDesc m_depthTargetDesc{};
    bool m_zeroLights = false;
    int m_viewPreset = 0;
    asset::SceneData m_scene;
    MaterialGpuVariantPlan m_variantPlan;
    std::unique_ptr<ForwardProgram> m_program;
    std::unique_ptr<ForwardRenderer> m_renderer;
    std::unique_ptr<PBREnvironmentResources> m_environment;
    std::vector<std::unique_ptr<MaterialGpuResources>> m_materialVariants;
    std::unique_ptr<GpuModelAsset> m_model;
    int m_selectedInstance = 0;
};
} // namespace ku
