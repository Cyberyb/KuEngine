#pragma once
#include "DeferredCommon.h"
#include "RenderPass.h"

namespace ku {
// Reusable Graph MRT/depth adapter. Scene ownership and draw assembly remain
// with the caller; use ForwardProgram::initializeGeometry + ForwardRenderer.
class DeferredGeometryPass : public RenderPass {
public:
    [[nodiscard]] std::string_view name() const override { return "DeferredGeometry"; }
    void initialize(const RenderContext& context) override;
    void setup(RenderGraphBuilder& builder) override;
    void execute(CommandList& commands, const FrameData& frame) override = 0;
protected:
    GBufferTargets m_gbufferTargets{};
};
} // namespace ku
