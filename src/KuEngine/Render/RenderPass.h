// KuEngine 渲染 Pass 模块：定义单个渲染阶段的声明、执行、界面绘制与尺寸变化扩展接口。
#pragma once

#include <cstdint>
#include <string_view>
#include <memory>
#include <array>

namespace ku {

class RHIDevice;
class CommandList;
class RenderGraphBuilder;

struct FrameData {
    uint32_t frameIndex;
    uint32_t imageIndex;
    float    deltaTime;
    float    totalTime;
};

class RenderPass {
public:
    virtual ~RenderPass() = default;

    [[nodiscard]] virtual std::string_view name() const = 0;
    [[nodiscard]] bool enabled() const { return m_enabled; }
    void setEnabled(bool e) { m_enabled = e; }

    virtual void initialize(RHIDevice& device) {(void)device; }
    virtual void setup() {}
    virtual void setup(RenderGraphBuilder& builder) {(void)builder; setup(); }
    virtual void update(const FrameData& frame) {(void)frame; }
    virtual void execute(CommandList& cmd, const FrameData& frame) {(void)cmd; (void)frame; }
    virtual void drawUI() {}
    virtual bool supportsInlineUI() const { return false; }
    virtual void drawUIInline() { drawUI(); }
    virtual void onResize(uint32_t width, uint32_t height) {(void)width; (void)height; }

protected:
    bool m_enabled = true;
};

} // namespace ku
