# UI 层架构与自定义开发指南

本文档面向 KuEngine 当前实现，回答两个核心问题：

1. UI 层在框架中的职责边界是什么。
2. 如何按框架约定编写自定义 UI。

## 1. UI 层架构概览

KuEngine 的 UI 设计采用两层分工：

- 框架层 UI：由 UIOverlay 统一管理 ImGui 后端生命周期与每帧提交。
- 业务层 UI：由各 RenderPass 的 drawUI 接口暴露参数面板。

对应关系如下：

- UIOverlay: [src/KuEngine/UI/UIOverlay.h](src/KuEngine/UI/UIOverlay.h), [src/KuEngine/UI/UIOverlay.cpp](src/KuEngine/UI/UIOverlay.cpp)
- RenderPass 接口: [src/KuEngine/Render/RenderPass.h](src/KuEngine/Render/RenderPass.h)
- RenderPipeline UI 分发: [src/KuEngine/Render/RenderPipeline.cpp](src/KuEngine/Render/RenderPipeline.cpp)
- 示例接入: [examples/triangle/main.cpp](examples/triangle/main.cpp)

### 1.1 UIOverlay 的职责

UIOverlay 负责通用 UI 基础设施，不负责具体业务参数定义：

- 初始化和销毁 ImGui。
- 初始化 GLFW + Vulkan 后端。
- 每帧 newFrame 与渲染提交。
- 通用面板绘制（例如 FPS）。
- swapchain 重建后的 UI 运行时更新。

换句话说，UIOverlay 是平台层和渲染后端适配层。

### 1.2 RenderPass::drawUI 的职责

RenderPass::drawUI 负责具体渲染功能的调参界面：

- 只操作本 Pass 拥有的参数。
- 不直接管理 ImGui 上下文和后端。
- 与 execute 共享同一份状态数据。

例如 TrianglePass 的颜色调节在 drawUI 中编辑，而 execute 在同帧读取该参数用于绘制。

## 2. 每帧调用链

当前推荐调用顺序（见示例主循环）：

1. uiOverlay.newFrame()
2. uiOverlay.drawFPSPanel(...)
3. renderPipeline.drawUI()
4. 进入渲染命令录制和 Pass execute
5. uiOverlay.render(cmd, ...)

这个顺序保证：

- 所有 Pass 的 UI 数据在本帧 execute 前就已更新。
- UI 与场景绘制共享同一个 command buffer 提交。

## 3. 如何编写自定义 UI

下面给出推荐流程。

### 步骤 1：在 Pass 中定义可调参数

在你的 RenderPass 子类中新增成员变量，例如曝光、颜色、开关等。

建议：

- 参数命名使用业务语义，不要写成 tmp1/tmp2。
- 给出合理默认值，确保首次启动即可运行。

### 步骤 2：在 drawUI 中绘制控件

在 Pass 的 drawUI 中调用 ImGui 控件并直接写入成员变量。

示意：

```cpp
void MyPass::drawUI() {
    ImGui::Begin("MyPass Controls");
    ImGui::Checkbox("Enable Bloom", &m_enableBloom);
    ImGui::SliderFloat("Exposure", &m_exposure, 0.1f, 5.0f);
    ImGui::ColorEdit4("Tint", m_tintColor.data());
    ImGui::End();
}
```

### 步骤 3：在 execute 中消费参数

execute 中读取同一份参数，写入 push constant、uniform 或其他 GPU 资源。

示意：

```cpp
void MyPass::execute(CommandList& cmd, const FrameData&) {
    if (!m_pipeline) return;

    m_pipeline->bind(cmd);
    vkCmdPushConstants(cmd, m_pipeline->layout(), VK_SHADER_STAGE_FRAGMENT_BIT,
        0, sizeof(MyPushConstants), &m_pushConstants);
    vkCmdDraw(cmd, 3, 1, 0, 0);
}
```

### 步骤 4：在主循环保持统一入口

不要在示例主循环里散落业务控件。主循环只保留两类调用：

- UIOverlay 的通用接口。
- RenderPipeline 的 drawUI 分发接口。

可参考 [examples/triangle/main.cpp](examples/triangle/main.cpp) 的当前组织方式。

## 4. 推荐约束与实践

### 4.1 建议遵守

- 通用信息面板放在 UIOverlay（FPS、帧时、全局状态）。
- Pass 私有参数放在各自 drawUI。
- drawUI 尽量只改 CPU 侧状态，不在里面创建/销毁重资源。
- 资源重建逻辑放在 initialize/onResize，不放在 drawUI。

### 4.2 避免的做法

- 在 main 中直接写大量业务 ImGui 控件。
- 在多个 Pass 之间互相直接修改彼此参数。
- 在 drawUI 中发起复杂 GPU 资源生命周期操作。

## 5. 扩展建议

如果后续 Pass 数量增加，可按下面方式进一步演进：

- 给 RenderPipeline::drawUI 增加分组或折叠头。
- 提供统一的 PassUIState 基类，规范标题、启用开关、重置按钮。
- 将通用调试面板（帧时间曲线、GPU 时间统计）集中到 UIOverlay。

通过这种分层，KuEngine 可以同时保持：

- 框架层实现稳定。
- 业务层 UI 扩展快速。
- 示例代码可读、可维护。
