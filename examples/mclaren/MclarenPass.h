// Mclaren 示例 Pass：只负责 RenderGraph 声明、逐帧编排、Draw Item 构造与调试 UI。
#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#ifndef GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#endif
#include <glm/vec3.hpp>

#include <KuEngine/Asset/AssetConfig.h>
#include <KuEngine/Render/RenderPass.h>

#include "MclarenAssetReplacement.h"
#include "OrbitCameraController.h"

namespace ku {

class RHIDevice;
class ForwardProgram;
class ForwardRenderer;
struct MclarenEnvironmentState;
struct MclarenModelState;

class MclarenPass final : public RenderPass {
public:
    explicit MclarenPass(
        std::vector<MclarenStartupReplacement> startupReplacements = {},
        uint64_t replaceAfterUpdates = 1);
    ~MclarenPass() override;

    [[nodiscard]] std::string_view name() const override
    {
        return "Mclaren";
    }

    void initialize(const RenderContext& context) override;
    void setup(RenderGraphBuilder& builder) override;
    void update(const FrameData& frame) override;
    void execute(CommandList& cmd, const FrameData& frame) override;
    void drawUI() override;
    [[nodiscard]] std::optional<CommandListStatistics>
    expectedFrameStatistics() const override;

    void addRotation(float deltaYaw, float deltaPitch);
private:
    [[nodiscard]] std::unique_ptr<MclarenEnvironmentState>
    buildEnvironmentCandidate(
        const std::filesystem::path& path,
        std::string& errorMessage,
        MclarenReplacementStage& failedStage,
        MclarenReplacementCategory& failedCategory);
    [[nodiscard]] std::unique_ptr<MclarenModelState> buildModelCandidate(
        const std::filesystem::path& path,
        std::string& errorMessage,
        MclarenReplacementStage& failedStage,
        MclarenReplacementCategory& failedCategory);
    void processPendingReplacement();
    void publishReplacementFailure(
        const MclarenAssetRequest& request,
        MclarenReplacementStage stage,
        MclarenReplacementCategory category,
        const std::string& message);
    void queueStartupReplacement();
    void resetAssetStates() noexcept;

    RHIDevice* m_device = nullptr;
    ImageDesc m_colorTargetDesc{};
    ImageDesc m_depthTargetDesc{};

    // Reverse destruction: renderer descriptor sets before program layouts.
    std::unique_ptr<ForwardProgram> m_forwardProgram;
    std::unique_ptr<ForwardRenderer> m_forwardRenderer;
    std::unique_ptr<MclarenEnvironmentState> m_environmentState;
    std::unique_ptr<MclarenModelState> m_modelState;
    OrbitCameraController m_camera;
    ViewerLayout m_viewerLayout;

    MclarenReplacementController m_replacement;
    std::vector<MclarenStartupReplacement> m_startupReplacements;
    size_t m_nextStartupReplacement = 0;
    uint64_t m_replaceAfterUpdates = 1;
    uint64_t m_completedNormalUpdates = 0;
    std::array<char, 1024> m_modelDraft{};
    std::array<char, 1024> m_environmentDraft{};
    std::string m_queueError;
    glm::vec3 m_lightDirection{0.35f, 1.0f, 0.45f};
    glm::vec3 m_lightColor{1.0f, 1.0f, 1.0f};
    float m_lightIntensity = 1.0f;
    std::vector<asset::PointLightConfig> m_pointLights;

    bool m_enableTextureSampling = true;
    bool m_enableNormalMap = true;
    bool m_enableOrmMap = true;
    bool m_enableEnvironmentMap = true;
    bool m_enableSkybox = true;
    bool m_flipUvY = true;
    bool m_enableOutputGamma = true;
    float m_environmentIntensity = 0.7f;
    float m_environmentExposure = 1.0f;
};

} // namespace ku
