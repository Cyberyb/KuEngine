# RenderPass 接口设计

## 1. 概述

RenderPass 是渲染框架的核心抽象，每个渲染算法（几何 Pass、光照 Pass、后处理 Pass 等）都是一个独立的 RenderPass。

设计参考了：
- Frostbite Engine (EA) - GDC 2017 Frame Graph
- Granite (Themaister) - 个人开源渲染框架
- Falcor (NVIDIA Research) - 学术研究框架

## 2. 接口设计

```cpp
struct RenderPass {
    virtual ~RenderPass() = default;

    // ========== 元信息 ==========
    [[nodiscard]] virtual std::string_view name() const = 0;
    [[nodiscard]] bool enabled() const { return m_enabled; }
    void setEnabled(bool e) { m_enabled = e; }

    // ========== 生命周期 ==========
    virtual void initialize(RHIDevice& device) { (void)device; }
    virtual void setup() {}
    virtual void setup(RenderGraphBuilder& builder) {
        (void)builder;
        setup(); // 兼容旧 Pass：未迁移时仍调用原 setup()
    }

    // ========== 执行阶段 ==========
    virtual void execute(CommandList& cmd, const FrameData& frame) {}

    // ========== UI 阶段 ==========
    virtual void drawUI() {}

    virtual void onResize(uint32_t width, uint32_t height) {}

protected:
    bool m_enabled = true;
};
```

## 3. TrianglePass 示例

MVP 阶段唯一的 Pass，用于渲染可见几何三角形：

```cpp
class TrianglePass : public RenderPass {
public:
    TrianglePass();
    ~TrianglePass() override;

    [[nodiscard]] std::string_view name() const override { return "Triangle"; }
    
    void initialize(RHIDevice& device) override;
    void execute(CommandList& cmd, const FrameData& frame) override;
    void drawUI() override;
    void onResize(uint32_t width, uint32_t height) override;

private:
    std::unique_ptr<RHIShader> m_vertShader;
    std::unique_ptr<RHIShader> m_fragShader;
    std::unique_ptr<RHIPipeline> m_pipeline;
    
    std::array<float, 4> m_triangleColor = {1.0f, 0.45f, 0.15f, 1.0f};
};
```

## 4. MVP 中的当前用法

v0.2 阶段1中，RenderPipeline 已接入 RenderGraph 的声明桥接层：

```cpp
void RenderPipeline::compile(RHIDevice& device) {
    m_renderGraph.reset();

    for (auto& pass : m_passes) {
        pass->initialize(device);

        const size_t graphIndex = m_renderGraph.registerPass(*pass);
        auto builder = m_renderGraph.buildPass(graphIndex);
        pass->setup(builder);
    }

    m_renderGraph.compile();
}

void RenderPipeline::execute(CommandList& cmd, const FrameData& frame) {
    for (RenderPass* pass : m_compiledExecution) {
        if (pass->enabled()) {
            pass->execute(cmd, frame);
        }
    }
}

void RenderPipeline::drawUI() {
    for (auto& pass : m_passes) {
        if (pass->enabled()) {
            pass->drawUI();
        }
    }
}
```

## 5. 后续扩展（RenderGraph）

v0.2 阶段1已提供最小声明接口：

```cpp
// RenderGraphBuilder - 用于在 setup() 中声明资源
class RenderGraphBuilder {
public:
    // 创建图内资源
    ResourceHandle createResource(std::string_view name);

    // 引用外部资源（例如 SwapChainColor）
    ResourceHandle importExternal(std::string_view name);

    // 声明读取的资源
    void read(ResourceHandle handle);

    // 声明写入的资源
    void write(ResourceHandle handle);

    // 显式声明 Pass 依赖（按名称）
    void dependsOn(std::string_view passName);
};

// 示例
void TrianglePass::setup(RenderGraphBuilder& builder) {
    auto swapChainColor = builder.importExternal("SwapChainColor");
    builder.write(swapChainColor);
}
```

v0.2 阶段2已完成以下编译能力：

- 基于资源读写冲突自动生成依赖边。
- 支持 `dependsOn()` 的显式依赖边。
- 使用拓扑排序生成执行顺序。
- 检测并报告循环依赖。
- 生成基础 barrier 计划数据（用于阶段3执行器接入）。

v0.2 阶段3已完成以下执行器对接：

- `RenderPipeline::execute` 可消费 barrier 计划。
- 对已绑定的外部图像资源，执行期通过 `CommandList::imageBarrier` 发射屏障。
- 示例层 swapchain 布局转换已统一收敛到 `CommandList` 屏障接口。

v0.2 阶段5已完成以下可观测补强：

- `RenderPipeline::drawUI` 提供 `RenderGraph Debug` 面板。
- 面板可查看 compile 摘要、依赖边、barrier 计划与最近一帧执行摘要。
- 执行期 barrier 事件支持区分 applied 与 skipped 原因。

## 6. RenderPass 注册机制

```cpp
class RenderPipeline {
public:
    template<typename T>
    T& addPass() {
        static_assert(std::is_base_of_v<RenderPass, T>);
        auto pass = std::make_unique<T>();
        T* ptr = pass.get();
        m_passes.push_back(std::move(pass));
        return *ptr;
    }

    void compile(RHIDevice& device) {
        m_renderGraph.reset();

        for (auto& pass : m_passes) {
            pass->initialize(device);

            const size_t graphIndex = m_renderGraph.registerPass(*pass);
            auto builder = m_renderGraph.buildPass(graphIndex);
            pass->setup(builder);
        }

        m_renderGraph.compile();
    }

    void execute(CommandList& cmd, const FrameData& frame) {
        for (auto& pass : m_passes) {
            if (pass->enabled()) {
                pass->execute(cmd, frame);
            }
        }
    }

    void drawUI() {
        for (auto& pass : m_passes) {
            if (pass->enabled()) {
                pass->drawUI();
            }
        }
    }

private:
    std::vector<std::unique_ptr<RenderPass>> m_passes;
};
```
