#pragma once

#include <string_view>
#include <memory>
#include <array>

// TrianglePass 在 examples/ 目录下，通过 src/ 路径引用核心库
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
    void execute(CommandList& cmd, const FrameData& frame) override;
    void drawUI() override;
    void onResize(uint32_t width, uint32_t height) override;

    void setClearColor(float r, float g, float b, float a) {
        m_clearColor = {r, g, b, a};
    }

    [[nodiscard]] std::array<float, 4> clearColor() const {
        return {m_clearColor[0], m_clearColor[1], m_clearColor[2], m_clearColor[3]};
    }

private:
    std::unique_ptr<RHIShader>  m_vertShader;
    std::unique_ptr<RHIShader>  m_fragShader;
    std::unique_ptr<RHIPipeline> m_pipeline;

    std::array<float, 4> m_clearColor = {0.05f, 0.05f, 0.05f, 1.0f};
    float m_rotationSpeed = 0.0f;
};

} // namespace ku
