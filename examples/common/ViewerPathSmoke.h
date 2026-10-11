#pragma once
#include <KuEngine/Core/Engine.h>
#include <KuEngine/Render/ScenePasses.h>
#include <functional>
#include <filesystem>
#include <memory>
namespace ku {
class RHIBuffer;
class ViewerPathReadback;
// Deliberately example-side, bounded verification of the real UI request API.
// No scenario machinery is added to Engine/Runtime.
class ViewerPathSmoke final : public RenderPass {
public:
    ViewerPathSmoke(Engine&,SceneDrawSource&,std::filesystem::path prefix,std::function<void(bool)> variant,uint64_t startAfter=0,std::function<std::vector<uint64_t>()> state={});
    ~ViewerPathSmoke() override;
    std::string_view name() const override {return "ViewerPathSmoke";}
    void bind(RenderPass& sceneUnorm,RenderPass& sceneSrgb,RenderPass& debugUnorm,RenderPass& debugSrgb,ViewerPathReadback&,ViewerPathReadback&);
    void update(const FrameData&) override;
    void setup(RenderGraphBuilder& b) override { m_source.declarePrepareDependency(b); }
    void drawUI() override;
    void execute(CommandList&,const FrameData&) override;
    void onResize(uint32_t,uint32_t) override;
    std::optional<CommandListStatistics> expectedFrameStatistics() const override {return CommandListStatistics{0,0};}
    struct Selection {asset::RenderPath path;GBufferView view;bool variant;};
    const Selection& selection() const;
    bool active() const {return m_active;}
    size_t step() const {return m_step;}
    const std::filesystem::path& prefix() const {return m_prefix;}
    uint64_t completedSerial() const;
    void acceptSample(size_t step,uint64_t recordedAfter,VkExtent2D extent,const uint8_t* bytes,bool debug);
private:
    Engine& m_engine;
    SceneDrawSource& m_source;
    std::filesystem::path m_prefix;
    std::function<void(bool)> m_variant;
    std::function<std::vector<uint64_t>()> m_state;
    uint64_t m_startAfter,m_requestedGeneration=0;
    size_t m_step=0;
    bool m_requested=false,m_active=false,m_done=false;
    std::vector<Selection> m_steps;
    std::array<RenderPass*,4> m_probes{};
    std::array<ViewerPathReadback*,2> m_readbacks{};
    std::vector<uint64_t> m_identity;
    std::vector<uint64_t> m_projection;
    VkExtent2D m_sceneExtent{};
    std::vector<uint8_t> m_debugBaseline;
    std::vector<uint16_t> m_hdrBaseline;
};
class ViewerPathReadback final : public RenderPass {
public:
    ViewerPathReadback(Engine&,ViewerPathSmoke&,bool debug);
    ~ViewerPathReadback() override;
    std::string_view name()const override{return m_debug?"ViewerDebugReadback":"ViewerSceneReadback";}
    bool requiresOutsideRenderingScope()const noexcept override{return true;}
    void initialize(const RenderContext&) override;
    void setup(RenderGraphBuilder&) override;
    void update(const FrameData&) override;
    void execute(CommandList&,const FrameData&,RenderGraphResourceResolver&) override;
    void inspect();
    std::optional<CommandListStatistics> expectedFrameStatistics()const override{return CommandListStatistics{0,0};}
private:
    Engine& m_engine;
    ViewerPathSmoke& m_owner;
    bool m_debug,m_pending=false;
    GBufferTargets m_targets{};
    ImageDesc m_hdr{};
    std::array<ImageHandle,7> m_images{};
    BufferHandle m_buffer{};
    std::unique_ptr<RHIBuffer> m_staging;
    VkExtent2D m_extent{};
    size_t m_step=0;
    uint64_t m_recordedAfter=0;
};
void addViewerPathSmoke(Engine&,SceneDrawSource&,std::filesystem::path,std::function<void(bool)>,uint64_t startAfter=0,std::function<std::vector<uint64_t>()> state={});
}
