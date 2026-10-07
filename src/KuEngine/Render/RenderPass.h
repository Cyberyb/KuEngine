// KuEngine 渲染 Pass 模块：定义单个渲染阶段的声明、执行、界面绘制与尺寸变化扩展接口。
#pragma once

#include <cstdint>
#include <string_view>
#include <memory>
#include <optional>
#include <array>

#include "../Core/FrameStatistics.h"
#include "../Core/ViewerLayout.h"
#include "RenderContext.h"

namespace ku {

class CommandList;
class RenderGraphBuilder;
class RenderGraphResourceResolver;

struct FrameData {
    uint32_t frameIndex;
    uint32_t imageIndex;
    float    deltaTime;
    float    totalTime;
    ViewerLayout viewerLayout{};
};

enum class PassExecutionModel {
    // Compatibility path: the object pass receives the full CommandList.
    TrustedLegacyObject,
    // Callback path: execution receives only GraphCommandContext.
    RestrictedCallback,
};

class RenderPass {
public:
    virtual ~RenderPass() = default;

    [[nodiscard]] virtual std::string_view name() const = 0;
    [[nodiscard]] virtual PassExecutionModel executionModel() const noexcept
    {
        return PassExecutionModel::TrustedLegacyObject;
    }
    [[nodiscard]] virtual bool requiresOutsideRenderingScope() const noexcept
    {
        return false;
    }
    [[nodiscard]] bool enabled() const { return m_enabled; }
    void setEnabled(bool e) { m_enabled = e; }

    virtual void initialize(const RenderContext& context) {(void)context; }
    virtual void setup() {}
    virtual void setup(RenderGraphBuilder& builder) {(void)builder; setup(); }
    virtual void prepare(const RenderContext& context) {(void)context; }
    virtual void update(const FrameData& frame) {(void)frame; }
    virtual void execute(CommandList& cmd, const FrameData& frame) {(void)cmd; (void)frame; }
    virtual void execute(
        CommandList& cmd,
        const FrameData& frame,
        RenderGraphResourceResolver& resources)
    {
        (void)resources;
        execute(cmd, frame);
    }
    // Draw this pass's controls inside the container owned by UIOverlay.
    virtual void drawUI() {}
    virtual void onResize(uint32_t width, uint32_t height) {(void)width; (void)height; }
    [[nodiscard]] virtual std::optional<CommandListStatistics>
    expectedFrameStatistics() const
    {
        return std::nullopt;
    }

protected:
    bool m_enabled = true;
};

} // namespace ku
