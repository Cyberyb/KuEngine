#pragma once

#include <array>
#include <memory>
#include <string>
#include <string_view>

#include <KuEngine/Render/RenderPass.h>
#include <KuEngine/RHI/CommandList.h>
#include <KuEngine/RHI/RHIDevice.h>
#include <KuEngine/RHI/RHIShader.h>
#include <KuEngine/RHI/RHIPipeline.h>

namespace ku {

class AlphaShapePass : public RenderPass {
public:
    AlphaShapePass(
        std::string name,
        std::array<float, 4> color,
        std::array<float, 2> offset,
        std::array<float, 2> scale,
        std::string dependency = {});
    ~AlphaShapePass() override;

    [[nodiscard]] std::string_view name() const override { return m_name; }

    void initialize(RHIDevice& device) override;
    void setup(RenderGraphBuilder& builder) override;
    void execute(CommandList& cmd, const FrameData& frame) override;
    void drawUI() override;
    void onResize(uint32_t width, uint32_t height) override;

private:
    struct alignas(16) PushConstants {
        float color[4];
        float offset[2];
        float scale[2];
    };

    std::string m_name;
    std::string m_dependency;

    std::unique_ptr<RHIShader> m_vertShader;
    std::unique_ptr<RHIShader> m_fragShader;
    std::unique_ptr<RHIPipeline> m_pipeline;

    std::array<float, 4> m_color;
    std::array<float, 2> m_offset;
    std::array<float, 2> m_scale;
};

} // namespace ku
