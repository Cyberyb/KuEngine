# UI 与性能统计设计

核对日期：2026-09-12。

源码：[UIOverlay](../../src/KuEngine/UI/UIOverlay.h)、[Engine::render](../../src/KuEngine/Core/Engine.cpp)、[CommandList](../../src/KuEngine/RHI/CommandList.cpp)。

## UI 所有权

Engine 持有单个 UIOverlay。UIOverlay 创建 ImGui Context、GLFW/Vulkan Backend、DescriptorPool，提供 newFrame、render、drawFPSPanel 和交换链 imageCount 更新。析构时关闭 Backend 和 Context。

Pass::drawUI 管业务参数；RenderPipeline::drawUI 提供 Graph 调试面板及 Pass UI 入口，supportsInlineUI 决定是否内嵌。Render 模块目前直接依赖 ImGui。

UIOverlay::render 调用 ImGui::Render 和 Vulkan Backend。imageView/imageLayout 参数当前未使用，实际附件由 RenderPipeline::executeOverlay 管理。

## 帧内调用位置

```text
UIOverlay::newFrame
→ RenderPipeline::update / drawUI
→ RenderPipeline::execute（业务命令）
→ UIOverlay::drawFPSPanel（构建统计控件）
→ RenderPipeline::executeOverlay
→ UIOverlay::render（生成 DrawData 并记录 ImGui 命令）
```

因此业务控件在绘制前更新参数，统计面板在业务绘制后读取当帧 draw 数。ImGui 最终 Render 前仍可增加统计控件。

## UIFrameStatistics 数据契约

drawStats 和 drawFPSPanel 接收 UIFrameStatistics，UI 只显示传入数据，不自行计算帧时间。

| 显示项 | 数据来源 | 当前含义 |
|---|---|---|
| FPS | Engine deltaTime 倒数 | 主循环相邻时间点的瞬时帧率 |
| Frame | deltaTime × 1000 | 主循环帧间隔，包含等待/呈现等影响 |
| CPU | Engine acquire 成功后到 submit 返回的墙钟差 | 最近已提交并在下一轮等待完成的帧；含 UI、更新、命令记录和提交 |
| GPU | CommandList 两个 Timestamp Query | 最近读回的主命令缓冲执行跨度，含 UI 和布局转换 |
| Draw Calls | CommandListStatistics | 当前帧通过 draw/drawIndexed 记录的业务命令数 |
| Vertices | CommandListStatistics | 当前帧 vertexCount 或 indexCount 乘 instanceCount 的总和 |

CPU 不含事件轮询、Fence/acquire 等待与 present；也不是进程 CPU 利用率或纯线程运行时间。GPU 不含显示器刷新延迟，可能包含时间戳范围内的 GPU 调度/同步停顿。两个指标不能直接相加当作 Frame。

## 可用性与显示限制

- 首帧 CPU/GPU 尚无结果，显示 N/A。
- 队列 timestampValidBits=0 时 GPU 显示 N/A。
- GPU 在 Fence 后读取 availability，避免用本帧尚未执行的 Query。
- CPU/GPU 标注 last completed frame；Draw Calls/Vertices 仍取当前记录帧，界面不是所有指标同一帧的完整快照。
- ImGui 内部 draw 不计入业务 Draw Calls/Vertices，但它的 GPU 命令包含在 GPU 时间内。
- 索引提交数量并不等于去重模型顶点数；Mclaren 控制面板中的 Mesh Vertices 是另一种资源容量统计。
- showStats=false 只隐藏面板，当前代码仍执行计数和计时。
- 当前没有时间平滑、历史曲线、CSV 导出、每 Pass GPU Query 或自动性能回归。

## 交互边界

ImGui Backend 使用 GLFW 回调接入；Mclaren 通过 WantCaptureMouse 避免在 UI 操作时旋转模型，滚轮取自 ImGuiIO。当前只有单 Context，未建立多窗口 UI 管理。
