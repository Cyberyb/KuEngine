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

class TrianglePass : public RenderPass {
public:
    TrianglePass();
    ~TrianglePass() override;

    [[nodiscard]] std::string_view name() const override { return "Triangle"; }
    
    void initialize(RHIDevice& device) override;
    void setup(RenderGraphBuilder& builder) override;
    void execute(CommandList& cmd, const FrameData& frame) override;
    void drawUI() override;
    void onResize(uint32_t width, uint32_t height) override;

    void setTriangleColor(float r, float g, float b, float a) {
        m_triangleColor = {r, g, b, a};
    }

    [[nodiscard]] std::array<float, 4> triangleColor() const {
        return {m_triangleColor[0], m_triangleColor[1], m_triangleColor[2], m_triangleColor[3]};
    }

private:
    std::unique_ptr<RHIShader>   m_vertShader;
    std::unique_ptr<RHIShader>   m_fragShader;
    std::unique_ptr<RHIPipeline> m_pipeline;

    std::array<float, 4> m_triangleColor = {0.2f, 0.4f, 0.9f, 1.0f};
    float m_rotationSpeed = 0.0f;
};

} // namespace ku
