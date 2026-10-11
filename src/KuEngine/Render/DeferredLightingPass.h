#pragma once
#include "DeferredCommon.h"
#include "RenderPass.h"
#include <functional>
#include <memory>

namespace ku {
class RHIBuffer;
class RHIParameterSet;
class RHIShader;
class RHIPipeline;
class DeferredLightingPass final : public RenderPass {
public:
    using ViewProvider = std::function<ForwardView(const FrameData&)>;
    explicit DeferredLightingPass(ViewProvider provider, std::string prepareDependency = {});
    ~DeferredLightingPass() override;
    [[nodiscard]] std::string_view name() const override { return "DeferredLighting"; }
    void initialize(const RenderContext& context) override;
    void setup(RenderGraphBuilder& builder) override;
    void execute(CommandList&, const FrameData&, RenderGraphResourceResolver&) override;
    [[nodiscard]] std::optional<CommandListStatistics> expectedFrameStatistics() const override
    { return CommandListStatistics{1, 3}; }
private:
    struct State;
    ViewProvider m_viewProvider;
    std::string m_prepareDependency;
    GBufferTargets m_targets{};
    ImageDesc m_outputDesc{};
    std::array<ImageHandle, gbufferAttachmentCount> m_images{};
    std::unique_ptr<State> m_state;
};
} // namespace ku
