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

当前版本不使用完整 RenderGraph，而是由 RenderPipeline 顺序调用每个 Pass：

```cpp
void RenderPipeline::compile(RHIDevice& device) {
    for (auto& pass : m_passes) {
        pass->initialize(device);
        pass->setup();
    }
}

void RenderPipeline::execute(CommandList& cmd, const FrameData& frame) {
    for (auto& pass : m_passes) {
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

在 v0.2 中将引入完整 RenderGraph，`setup()` 将扩展为可声明资源依赖：

```cpp
// RenderGraphBuilder - 用于在 setup() 中声明资源
class RenderGraphBuilder {
public:
    // 声明读取的资源
    void read(ResourceHandle handle);
    
    // 声明写入的资源
    void write(ResourceHandle handle);
    
    // 创建新资源
    ResourceHandle create(const ResourceDesc& desc);
    
    // 引用交换链图像
    ResourceHandle swapChainImage();
};

// 示例
void GeometryPass::setup(RenderGraphBuilder& builder) {
    m_outputColor = builder.create({
        .width = builder.swapChainImage().width(),
        .height = builder.swapChainImage().height(),
        .format = VK_FORMAT_R8G8B8A8_SRGB
    });
    
    m_depthBuffer = builder.create({
        .width = builder.swapChainImage().width(),
        .height = builder.swapChainImage().height(),
        .format = VK_FORMAT_D32_SFLOAT
    });
    
    builder.write(m_outputColor);  // 作为颜色输出
    builder.write(m_depthBuffer);  // 作为深度输出
}
```

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
        for (auto& pass : m_passes) {
            pass->initialize(device);
            pass->setup();
        }
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
