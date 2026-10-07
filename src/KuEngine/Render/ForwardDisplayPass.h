#pragma once

#include "ForwardDisplayProgram.h"
#include "RenderPass.h"

#include <memory>
#include <string_view>

namespace ku {

class ForwardDisplayPass final : public RenderPass {
public:
    [[nodiscard]] std::string_view name() const override
    {
        return "ForwardDisplay";
    }

    void initialize(const RenderContext& context) override;
    void setup(RenderGraphBuilder& builder) override;
    void execute(
        CommandList& commands,
        const FrameData& frame,
        RenderGraphResourceResolver& resources) override;
    [[nodiscard]] std::optional<CommandListStatistics>
    expectedFrameStatistics() const override
    {
        return CommandListStatistics{1, 3};
    }

private:
    ImageDesc m_sceneColorDesc{};
    ImageDesc m_swapChainDesc{};
    ImageHandle m_sceneColor{};
    std::unique_ptr<ForwardDisplayProgram> m_program;
};

} // namespace ku
