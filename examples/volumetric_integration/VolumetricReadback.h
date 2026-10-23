#pragma once
#include "VolumetricPass.h"
#include <KuEngine/RHI/RHIBuffer.h>
#include <filesystem>

namespace ku { class RenderPipeline; }
namespace ku::volumetric {
struct CaptureReport {
    uint64_t completed = 0;
    std::string error;
};
// Optional evidence pass. Runtime waits the rotating slot before update, and all
// slots before resize/recompile/destruction. Every capture carries the actual
// submittedFrame, viewport and push constants from the same GPU submission.
class ReadbackPass final : public RenderPass {
public:
    ReadbackPass(RenderPipeline&, ScenePass&, std::filesystem::path, std::shared_ptr<CaptureReport>);
    ~ReadbackPass() override;
    std::string_view name() const override { return "VolumetricReadback"; }
    bool requiresOutsideRenderingScope() const noexcept override { return true; }
    void initialize(const RenderContext&) override;
    void setup(RenderGraphBuilder&) override;
    void update(const FrameData&) override;
    void onResize(uint32_t,uint32_t) override;
    void execute(CommandList&,const FrameData&,RenderGraphResourceResolver&) override;
    std::optional<CommandListStatistics> expectedFrameStatistics() const override { return CommandListStatistics{0,0}; }
private:
    struct Capture {
        std::unique_ptr<RHIBuffer> staging;
        VkExtent2D extent{};
        ViewerPixelRect rect{};
        SceneConstants constants{};
        uint64_t submit = 0;
        bool pending = false;
    };
    void inspect(uint32_t slot);
    void inspectAll();
    RenderPipeline& m_pipeline;
    ScenePass& m_source;
    std::filesystem::path m_prefix;
    std::shared_ptr<CaptureReport> m_report;
    std::vector<Capture> m_captures;
    ImageHandle m_image;
    BufferHandle m_buffer;
};
} // namespace ku::volumetric
