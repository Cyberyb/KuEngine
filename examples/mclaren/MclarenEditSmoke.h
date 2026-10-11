#pragma once
#include <KuEngine/Core/Engine.h>
#include <KuEngine/Render/ScenePasses.h>
#include <filesystem>
#include <memory>
#include "MclarenPass.h"

namespace ku {
class MclarenEditReadback;
// Bounded example-only verification. Requests use the exact UI entry points;
// staging is consumed only after the actual completed submission advances.
class MclarenEditSmoke final : public RenderPass {
public:
    MclarenEditSmoke(Engine&, MclarenPass&, std::filesystem::path prefix);
    std::string_view name() const override { return "MclarenEditSmoke"; }
    void setup(RenderGraphBuilder& b) override { m_source.declarePrepareDependency(b); }
    void update(const FrameData&) override;
    void execute(CommandList&, const FrameData&) override;
    std::optional<CommandListStatistics> expectedFrameStatistics() const override { return CommandListStatistics{0,0}; }
    void bind(MclarenEditReadback&, MclarenEditReadback&);
    uint64_t completed() const;
    size_t step() const { return m_step; }
    bool capture() const { return m_capture; }
    void sample(bool mrt, VkExtent2D, const uint8_t*, size_t bytes, uint64_t after, size_t step);
private:
    void requestStep();
    void checkSnapshot(const SceneRenderSnapshot&);
    Engine& m_engine;
    MclarenPass& m_source;
    std::filesystem::path m_prefix;
    std::array<MclarenEditReadback*,2> m_readbacks{};
    size_t m_step = 0;
    bool m_requested=false, m_capture=false, m_hdrDone=false, m_mrtDone=false, m_done=false;
    uint64_t m_after=0, m_editGeneration=0, m_environmentGeneration=0, m_oldToken=0;
    std::vector<uint64_t> m_gpuIdentity, m_preserved;
    std::vector<ResolvedMaterial> m_previousMaterials;
    std::vector<glm::mat4> m_previousTransforms;
    std::array<uint32_t,4> m_captureViewport{};
    // Actual snapshot input, not a reference: VP followed by six model matrices.
    std::array<float,16+6*16> m_captureMatrices{};
};
void addMclarenEditSmoke(Engine&, MclarenPass&, const std::filesystem::path& prefix);
}
