#pragma once

#include "ForwardDisplayProgram.h"
#include "RenderPass.h"
#include "ForwardCommon.h"
#include <functional>

#include <memory>
#include <string_view>

namespace ku {

class ForwardDisplayPass final : public RenderPass {
public:
    explicit ForwardDisplayPass(bool deferredSource = false) : m_deferredSource(deferredSource) {}
    ForwardDisplayPass(std::function<ForwardView(const FrameData&)> viewProvider,
        std::string prepareDependency) : m_viewProvider(std::move(viewProvider)),
        m_prepareDependency(std::move(prepareDependency)) {}
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
    bool m_deferredSource = false;
    std::function<ForwardView(const FrameData&)> m_viewProvider;
    std::string m_prepareDependency;
    ImageDesc m_sceneColorDesc{};
    ImageDesc m_swapChainDesc{};
    ImageHandle m_sceneColor{};
    std::unique_ptr<ForwardDisplayProgram> m_program;
};

} // namespace ku
