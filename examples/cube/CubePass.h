#pragma once

#include <string_view>
#include <memory>
#include <array>

#include <KuEngine/RHI/CommandList.h>
#include <KuEngine/Render/RenderPass.h>
#include <KuEngine/RHI/RHIDevice.h>
#include <KuEngine/RHI/RHIShader.h>
#include <KuEngine/RHI/RHIPipeline.h>

namespace ku {

class CubePass : public RenderPass {
public:
    CubePass();
    ~CubePass() override;

    [[nodiscard]] std::string_view name() const override { return "Cube"; }

    void initialize(RHIDevice& device) override;
    void setup(RenderGraphBuilder& builder) override;
    void execute(CommandList& cmd, const FrameData& frame) override;
    void drawUI() override;
    void onResize(uint32_t width, uint32_t height) override;

    void addRotation(float deltaYaw, float deltaPitch);
    void setAspect(float aspect) { m_aspect = aspect; }

private:
    struct alignas(16) PushConstants {
        float mvp[16];
        float color[4];
        float params[4];
    };

    std::unique_ptr<RHIShader> m_vertShader;
    std::unique_ptr<RHIShader> m_fragShader;
    std::unique_ptr<RHIPipeline> m_solidPipeline;
    std::unique_ptr<RHIPipeline> m_wirePipeline;

    std::array<float, 4> m_cubeColor = {0.1f, 0.9f, 0.8f, 1.0f};
    bool m_wireframeMode = false;
    float m_yaw = 0.0f;
    float m_pitch = 0.0f;
    float m_distance = 3.5f;
    float m_aspect = 16.0f / 9.0f;
};

} // namespace ku
