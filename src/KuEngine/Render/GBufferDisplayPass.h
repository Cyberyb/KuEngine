#pragma once
#include "ScenePasses.h"
#include <memory>
#include <vector>
namespace ku {
// Narrow MRT display; optional fixed-format internal output is for bounded
// format-encoding tests. Graph owns every source/output image.
class GBufferDisplayPass final : public RenderPass {
public:
    explicit GBufferDisplayPass(SceneDrawSource& source, VkFormat output = VK_FORMAT_UNDEFINED, std::string name = "GBufferDisplay");
    ~GBufferDisplayPass() override;
    std::string_view name() const override { return m_name; }
    void initialize(const RenderContext&) override;
    void setup(RenderGraphBuilder&) override;
    void execute(CommandList&, const FrameData&, RenderGraphResourceResolver&) override;
    std::optional<CommandListStatistics> expectedFrameStatistics() const override { return CommandListStatistics{1,3}; }
private:
    struct State;
    SceneDrawSource& m_source;
    VkFormat m_fixedFormat;
    std::string m_name;
    GBufferTargets m_targets{};
    ImageDesc m_output{};
    std::array<ImageHandle,4> m_images{};
    std::vector<std::unique_ptr<State>> m_states;
};
}
