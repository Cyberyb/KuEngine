#pragma once

#include "RenderPass.h"
#include <array>
#include <memory>

namespace ku {

class RHIDevice;
class RHIShader;
class RHIPipeline;

class TrianglePass : public RenderPass {
public:
    TrianglePass();
    ~TrianglePass() override;

    [[nodiscard]] std::string_view name() const override { return "Triangle"; }
    
    void initialize(const RHIDevice& device) override;
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
    float m_rotationSpeed = 0.0f; // 简单匀速旋转（0 表示静止）
};

} // namespace ku
