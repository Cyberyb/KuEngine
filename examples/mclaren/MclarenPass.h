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
#include <KuEngine/Render/ScenePasses.h>

#include "MclarenAssetReplacement.h"
#include "MclarenSceneEditing.h"
#include "OrbitCameraController.h"

namespace ku {

class RHIDevice;
class ForwardProgram;
class ForwardRenderer;
struct MclarenEnvironmentState;
struct MclarenModelState;
struct MclarenEnvironmentLifetime;

class MclarenPass final : public RenderPass, public SceneDrawSource {
public:
    explicit MclarenPass(
        std::vector<MclarenStartupReplacement> startupReplacements = {},
        uint64_t replaceAfterUpdates = 1, bool deferred = false,
        std::optional<asset::RenderPath> pathOverride = std::nullopt,
        std::filesystem::path scenePath = {});
    ~MclarenPass() override;

    [[nodiscard]] std::string_view name() const override
    {
        return "MclarenScenePrepare";
    }

    void initialize(const RenderContext& context) override;
    void setup(RenderGraphBuilder& builder) override;
    void update(const FrameData& frame) override;
    void execute(CommandList& cmd, const FrameData& frame) override;
    void drawUI() override;
    [[nodiscard]] std::optional<CommandListStatistics>
    expectedFrameStatistics() const override;

    void addRotation(float deltaYaw, float deltaPitch);
    std::string_view scenePrepareName() const override { return name(); }
    const SceneRenderSnapshot& sceneSnapshot() const override { return m_snapshot; }
    bool sceneSourceEnabled() const override { return enabled(); }
    ForwardRenderer& sceneRenderer(SceneDrawPhase phase) override;
    void setPathSmokeVariant(bool variant) { m_pathSmokeVariant = variant; }
    void seedPathSmokeState() { m_seedPathSmoke = true; }
    // Example-side verification only: not a Runtime/renderer state-machine API.
    std::vector<uint64_t> pathSmokeState() const;
    bool requestEdit(MclarenSceneEdit edit);
    bool requestAsset(MclarenAssetKind kind, std::string path = {});
    const asset::SceneData& editableScene() const;
    uint64_t editGeneration() const { return m_editor.generation(); }
    uint64_t environmentToken() const;
    bool environmentTokenReleased(uint64_t token) const;
    const MclarenPublishedAssetState& publishedAssets() const { return m_replacement.live(); }
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
    void processPendingEdit();
    void publishReplacementFailure(
        const MclarenAssetRequest& request,
        MclarenReplacementStage stage,
        MclarenReplacementCategory category,
        const std::string& message);
    void queueStartupReplacement();
    void resetAssetStates() noexcept;

    RHIDevice* m_device = nullptr;
    std::function<void()> m_waitForAllFrames;
    ImageDesc m_colorTargetDesc{};
    ImageDesc m_depthTargetDesc{};

    // Reverse destruction: renderer descriptor sets before program layouts.
    std::unique_ptr<ForwardProgram> m_forwardProgram;
    std::unique_ptr<ForwardRenderer> m_forwardRenderer;
    std::unique_ptr<ForwardProgram> m_geometryProgram;
    std::unique_ptr<ForwardRenderer> m_geometryRenderer;
    std::unique_ptr<ForwardRenderer> m_blendRenderer;
    std::unique_ptr<ForwardRenderer> m_backgroundRenderer;
    SceneRenderSnapshot m_snapshot{};
    bool m_deferred = false;
    bool m_pathSmokeVariant = false;
    bool m_seedPathSmoke = false;
    std::optional<asset::RenderPath> m_pathOverride;
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
    std::filesystem::path m_explicitScene;
    MclarenSceneEditor m_editor;
    size_t m_selectedInstance = 0;
    uint32_t m_selectedSlot = 0;
    std::shared_ptr<MclarenEnvironmentLifetime> m_environmentLifetime;
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
    bool m_outputAttachmentSrgb = false;
    float m_environmentIntensity = 0.7f;
    float m_environmentExposure = 1.0f;
};

} // namespace ku
