#pragma once
#include <KuEngine/Render/RenderPass.h>
#include <KuEngine/Render/DeferredCommon.h>
#include <KuEngine/Render/RenderPipeline.h>
#include <KuEngine/Render/ScenePasses.h>
#include <KuEngine/Render/ForwardDisplayProgram.h>
#include <memory>
#include <filesystem>

namespace ku {
class RHIBuffer;
class HdrDisplayProbePass final : public RenderPass {
public:
    HdrDisplayProbePass(SceneDrawSource& source, bool srgb) : m_source(source), m_srgb(srgb) {}
    std::string_view name() const override { return m_srgb ? "HdrDisplaySRGB" : "HdrDisplayUNORM"; }
    void initialize(const RenderContext&) override;
    void setup(RenderGraphBuilder&) override;
    void execute(CommandList&, const FrameData&, RenderGraphResourceResolver&) override;
    std::optional<CommandListStatistics> expectedFrameStatistics() const override { return CommandListStatistics{1,3}; }
private:
    SceneDrawSource& m_source;
    bool m_srgb;
    ImageDesc m_scene{}, m_output{};
    ImageHandle m_input{};
    ForwardDisplayProgram m_program;
};
// Test-only, bounded single-frame staging. Graph owns and synchronizes source
// images; the imported staging buffer explicitly hands transfer writes to HOST.
class DeferredReadbackPass final : public RenderPass {
public:
    DeferredReadbackPass(RenderPipeline& pipeline, std::filesystem::path prefix, bool backface, bool deferred, bool semantic);
    ~DeferredReadbackPass() override;
    [[nodiscard]] std::string_view name() const override { return "DeferredReadback"; }
    [[nodiscard]] bool requiresOutsideRenderingScope() const noexcept override { return true; }
    void initialize(const RenderContext&) override;
    void setup(RenderGraphBuilder&) override;
    void update(const FrameData&) override;
    void onResize(uint32_t, uint32_t) override;
    void execute(CommandList&, const FrameData&, RenderGraphResourceResolver&) override;
    void setupDepthCapture(RenderGraphBuilder&);
    void captureDepth(CommandList&, RenderGraphResourceResolver&);
    void initializeDepthCapture(const RenderContext& c) { m_targets = makeGBufferTargets(c.depthFormat,c.clearDepthStencil); }
    [[nodiscard]] std::optional<CommandListStatistics> expectedFrameStatistics() const override
    { return CommandListStatistics{0, 0}; }
private:
    void allocate(uint32_t, uint32_t);
    void inspect();
    RenderPipeline& m_pipeline;
    const RHIDevice* m_device = nullptr;
    std::filesystem::path m_prefix;
    bool m_backface = false;
    bool m_pending = false;
    bool m_deferred = false, m_semantic = true;
    GBufferTargets m_targets{};
    ImageDesc m_color{};
    std::array<ImageHandle, 8> m_images{};
    ImageHandle m_depthBefore{};
    BufferHandle m_beforeBuffer{};
    BufferHandle m_buffer{};
    std::unique_ptr<RHIBuffer> m_staging;
    VkExtent2D m_extent{}, m_copiedExtent{};
};
class HdrDepthCapturePass final : public RenderPass {
public:
    HdrDepthCapturePass() = default;
    void setOwner(DeferredReadbackPass& owner) { m_owner = &owner; }
    std::string_view name() const override { return "HdrDepthBeforeBlend"; }
    bool requiresOutsideRenderingScope() const noexcept override { return true; }
    void initialize(const RenderContext& c) override { m_owner->initializeDepthCapture(c); }
    void setup(RenderGraphBuilder& b) override { m_owner->setupDepthCapture(b); }
    void execute(CommandList& c, const FrameData&, RenderGraphResourceResolver& r) override { m_owner->captureDepth(c,r); }
    std::optional<CommandListStatistics> expectedFrameStatistics() const override { return CommandListStatistics{0,0}; }
private:
    DeferredReadbackPass* m_owner = nullptr;
};
} // namespace ku
