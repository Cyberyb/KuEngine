#pragma once

#include <string_view>
#include <memory>
#include <array>

#include <KuEngine/Core/SceneInteraction.h>
#include <KuEngine/RHI/CommandList.h>
#include <KuEngine/Render/RenderPass.h>
#include <KuEngine/RHI/RHIDevice.h>
#include <KuEngine/RHI/RHIShader.h>
#include <KuEngine/RHI/RHIPipeline.h>

namespace ku {

inline constexpr float cubeCameraDefaultDistance = 3.5f;
inline constexpr float cubeCameraMinimumDistance = 2.0f;
inline constexpr float cubeCameraMaximumDistance = 8.0f;
inline constexpr float cubeCameraScrollStep = 0.35f;

[[nodiscard]] float adjustCubeCameraDistance(
    float currentDistance,
    double scrollY) noexcept;

class CubePass : public RenderPass {
public:
    CubePass();
    ~CubePass() override;

    [[nodiscard]] std::string_view name() const override { return "Cube"; }

    void initialize(const RenderContext& context) override;
    void setup(RenderGraphBuilder& builder) override;
    void update(const FrameData& frame) override;
    void execute(CommandList& cmd, const FrameData& frame) override;
    void drawUI() override;
    void onResize(uint32_t width, uint32_t height) override;
    [[nodiscard]] std::optional<CommandListStatistics>
    expectedFrameStatistics() const override
    {
        return CommandListStatistics{
            1,
            m_wireframeMode ? 24u : 36u,
        };
    }

    void addRotation(float deltaYaw, float deltaPitch);
    void applyViewerInteraction(const ScenePointerAction& interaction) noexcept;
    [[nodiscard]] float projectionAspect() const { return m_aspect; }
    [[nodiscard]] float cameraDistance() const { return m_distance; }

private:
    struct alignas(16) PushConstants {
        float mvp[16];
        float color[4];
        float params[4];
    };

    ImageDesc m_colorTargetDesc{};
    std::unique_ptr<RHIShader> m_vertShader;
    std::unique_ptr<RHIShader> m_fragShader;
    std::unique_ptr<RHIPipeline> m_solidPipeline;
    std::unique_ptr<RHIPipeline> m_wirePipeline;

    std::array<float, 4> m_cubeColor = {0.1f, 0.9f, 0.8f, 1.0f};
    bool m_wireframeMode = false;
    float m_yaw = 0.0f;
    float m_pitch = 0.0f;
    float m_distance = cubeCameraDefaultDistance;
    float m_aspect = 16.0f / 9.0f;
    SceneInteractionGate m_sceneInteraction;
};

} // namespace ku
