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
    [[nodiscard]] virtual bool enabled() const { return true; }

    // ========== 资源声明阶段 ==========
    // 在 RenderGraph::compile() 前调用，用于声明该 Pass 需要的输入输出资源
    virtual void setup(RenderGraphBuilder& builder) {}

    // ========== 执行阶段 ==========
    // 在主循环中调用，按拓扑顺序执行
    virtual void execute(CommandList& cmd, const FrameData& frame) {}

    // ========== UI 阶段 ==========
    // 在 ImGui 窗口中显示该 Pass 的控制面板
    virtual void drawUI() {}

    // ========== 生命周期 ==========
    // 在 Pass 首次执行前调用，用于初始化
    virtual void initialize(const RHIDevice& device) {}
    
    // 在窗口 resize 时调用
    virtual void onResize(uint32_t width, uint32_t height) {}
};
```

## 3. TrianglePass 示例

MVP 阶段唯一的 Pass，用于渲染全屏三角形：

```cpp
class TrianglePass : public RenderPass {
public:
    TrianglePass();
    ~TrianglePass() override;

    [[nodiscard]] std::string_view name() const override { return "Triangle"; }
    
    void initialize(const RHIDevice& device) override;
    void execute(CommandList& cmd, const FrameData& frame) override;
    void drawUI() override;
    void onResize(uint32_t width, uint32_t height) override;

    // 可调节参数
    void setClearColor(float r, float g, float b, float a) {
        m_clearColor = {r, g, b, a};
    }

private:
    std::unique_ptr<RHIShader>   m_vertexShader;
    std::unique_ptr<RHIShader>   m_fragmentShader;
    std::unique_ptr<RHIPipeline>  m_pipeline;
    
    std::array<float, 4> m_clearColor = {0.0f, 0.0f, 0.0f, 1.0f};
};
```

## 4. MVP 中的简化用法

MVP 阶段不使用完整的 RenderGraph，直接在主循环中调用：

```cpp
// 主循环
void run() {
    while (!m_quitting) {
        pollEvents();
        
        // 等待上一帧完成
        m_syncManager.waitForFrame(m_currentFrame);
        
        // 获取交换链图像
        uint32_t imageIndex = m_swapChain->acquireNextImage(
            m_syncManager.imageAvailable(m_currentFrame));
        
        // 录制命令
        m_commandList.begin();
        
        // 使用 Dynamic Rendering
        VkRenderingInfo renderingInfo{};
        renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
        renderingInfo.renderArea = {{0, 0}, m_swapChain->extent()};
        renderingInfo.layerCount = 1;
        renderingInfo.colorAttachmentCount = 1;
        renderingInfo.pColorAttachments = &colorAttachment;
        
        vkCmdBeginRendering(m_commandList, &renderingInfo);
        m_trianglePass.execute(m_commandList, frame);
        vkCmdEndRendering(m_commandList);
        
        m_commandList.end();
        
        // 提交
        m_syncManager.submit(m_currentFrame, m_device.graphicsQueue(), 
                            m_commandList);
        
        // 呈现
        m_syncManager.present(m_currentFrame, m_device.presentQueue(), 
                              imageIndex);
        
        m_currentFrame = (m_currentFrame + 1) % FRAMES_IN_FLIGHT;
    }
}
```

## 5. 后续扩展（RenderGraph）

在 v0.2 中将引入完整 RenderGraph，Pass 接口保持不变：

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
    void addPass() {
        static_assert(std::is_base_of_v<RenderPass, T>);
        m_passes.push_back(std::make_unique<T>());
    }

    void compile() {
        // 1. 拓扑排序
        // 2. 生成 barrier
        // 3. 初始化所有 Pass
        for (auto& pass : m_passes) {
            pass->initialize(m_device);
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
            ImGui::PushID(pass->name().data());
            if (ImGui::CollapsingHeader(pass->name().data())) {
                pass->drawUI();
            }
            ImGui::PopID();
        }
    }

private:
    std::vector<std::unique_ptr<RenderPass>> m_passes;
};
```
