#pragma once

#include <KuEngine/Render/RenderPass.h>
#include <KuEngine/Render/DeferredCommon.h>

#include <filesystem>
#include <memory>
#include <vector>

namespace ku {
class RenderPipeline;
class RHIBuffer;
class SceneDrawSource;
// Optional scene-local evidence: copies actual GPU base/model/HDR pixels into a
// per-slot staging buffer, inspected only after that slot's Runtime fence.
class CornellBoxReadback final : public RenderPass {
public:
    CornellBoxReadback(RenderPipeline&,SceneDrawSource&,std::filesystem::path);
    void verifyFrameIsolation(bool enabled) { m_verifyFrameIsolation = enabled; }
    ~CornellBoxReadback() override;
    std::string_view name() const override { return "CornellBoxReadback"; }
    bool requiresOutsideRenderingScope() const noexcept override { return true; }
    void initialize(const RenderContext&) override;
    void setup(RenderGraphBuilder&) override;
    void update(const FrameData&) override;
    void onResize(uint32_t,uint32_t) override;
    void execute(CommandList&,const FrameData&,RenderGraphResourceResolver&) override;
    std::optional<CommandListStatistics> expectedFrameStatistics() const override { return CommandListStatistics{0,0}; }
private:
    void inspect();
    RenderPipeline& m_pipeline;
    SceneDrawSource& m_source;
    std::filesystem::path m_prefix;
    struct Capture {
        std::unique_ptr<RHIBuffer> staging;
        VkExtent2D extent{};
        ViewerPixelRect rect{};
        bool pending = false;
        float exposure = 1;
        uint64_t submit = 0;
    };
    std::vector<Capture> m_frames;
    uint32_t m_activeFrame = 0;
    bool m_verifyFrameIsolation = false;
    std::array<ImageDesc,3> m_descriptors{};
    std::array<ImageHandle,3> m_images{};
    BufferHandle m_buffer{};

};
} // namespace ku
