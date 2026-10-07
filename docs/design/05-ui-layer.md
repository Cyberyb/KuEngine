# UI 与性能统计设计

核对日期：2026-10-07。

源码：[UIOverlay](../../src/KuEngine/UI/UIOverlay.h)、[Engine::render](../../src/KuEngine/Core/Engine.cpp)、[CommandList](../../src/KuEngine/RHI/CommandList.cpp)。

## UI 所有权

Engine 持有单个 UIOverlay。UIOverlay 创建 ImGui Context、GLFW/Vulkan Backend、DescriptorPool，唯一拥有 `SidebarState` 和 `SidebarLayoutPolicy`；提供 newFrame、render、右侧栏绘制和交换链 imageCount 更新。析构时关闭 Backend 和 Context。

Pass::drawUI 只构建业务参数内容；RenderPipeline 分别提供 Pass 内容和 Graph 调试内容，逐 Pass 以索引 `PushID` 隔离控件 ID。UIOverlay 将它们放进唯一右侧栏的 Performance、Parameters / Scene、Render Graph 区段，避免多个浮动窗口或 Alpha Pass 同名控件冲突。Render 模块目前直接依赖 ImGui。

Mclaren 的 Parameters / Scene 内另有 Model 与 HDR 区块：各自显示 Draft 路径、Load 按钮、Active 路径、generation、状态/分类和错误。Load 只将路径复制为不可变请求并禁用并发请求；它不在 ImGui 回调中读取文件、上传或销毁 GPU 对象。请求会在下一次可用的 Pass update 安全点处理，成功后刷新 Active/generation，失败时 Draft/状态可见而已发布的画面、统计和相机保持。

Mclaren 当前还显示共享 mesh 数、材质 GPU variant 数、draw 和各语义贴图数量，并提供材质采样、相机、方向/首个点光、环境和替换控件。ForwardReuse 显示语义/背面视图预设、零光状态、共享 MeshHandle、variant 数和实例选择信息。它们是示例调试 UI，不是通用实例编辑器；没有完成产品需求中的实例选择/逐实例编辑工作流。

UIOverlay::render 调用 ImGui::Render 和 Vulkan Backend。UI depth format 为 `VK_FORMAT_UNDEFINED`；Overlay 在 Display CLEAR/STORE 后对 SwapChainColor 使用 LOAD/read-write，实际附件由 RenderPipeline::executeOverlay 管理。

## 帧内调用位置

```text
UIOverlay::newFrame
→ RenderPipeline::update
→ UIOverlay::drawSidebar（Pass / Graph 内容）
→ RenderPipeline::execute（业务命令）
→ UIOverlay::drawSidebar（构建统计与内容控件）
→ RenderPipeline::executeOverlay
→ UIOverlay::render（生成 DrawData 并记录 ImGui 命令）
```

`describeSidebarFrame` 在帧初捕获布局、区段与保留宽度；该快照同时决定 `ViewerLayout`、输入排除区域和本帧侧栏，控件引发的收展只在下一帧生效。展开模式将右侧逻辑宽度留给侧栏；收起后为紧凑统计条，主动隐藏紧凑统计后只保留重开控件。小窗口仍允许滚动；过窄时场景保持完整并由侧栏覆盖。ImGui 最终 Render 前仍可增加统计控件。

## UIFrameStatistics 数据契约

`drawSidebar` 内部的统计内容接收 `UIFrameStatistics`，UI 只显示传入数据，不自行计算帧时间。

| 显示项 | 数据来源 | 当前含义 |
|---|---|---|
| FPS | Engine deltaTime 倒数 | 主循环相邻时间点的瞬时帧率 |
| Frame | deltaTime × 1000 | 主循环帧间隔，包含等待/呈现等影响 |
| CPU | Engine acquire 成功后到 submit 返回的墙钟差 | 最近完成提交；含 UI、更新、命令记录和提交 |
| GPU | CommandList 两个 Timestamp Query | 同一最近完成提交的主命令缓冲执行跨度，含 UI 和布局转换 |
| Draw Calls | CompletedFrameStatistics | 同一最近完成提交中 draw/drawIndexed 记录的业务命令数 |
| Vertices | CompletedFrameStatistics | 同一最近完成提交中 vertexCount 或 indexCount 乘 instanceCount 的总和 |

CPU 不含事件轮询、Fence/acquire 等待与 present；也不是进程 CPU 利用率或纯线程运行时间。GPU 不含显示器刷新延迟，可能包含时间戳范围内的 GPU 调度/同步停顿。两个指标不能直接相加当作 Frame。

## 可用性与显示限制

- FPS/Frame 标注 current main-loop sample；CPU/GPU/Draw Calls/Vertices 标注 same completed submit，首个完成快照前显示 waiting/N/A。
- GPU `Unsupported` 表示没有 Timestamp 能力，`Waiting` 表示支持但尚无可读查询，`Available` 才显示毫秒值。
- GPU 在 Fence 后读取 availability，避免用本帧尚未执行的 Query；有限帧结束在 device idle 后也会尝试完成 pending 快照。
- ImGui 内部 draw 不计入业务 Draw Calls/Vertices，但它的 GPU 命令包含在 GPU 时间内。
- 索引提交数量并不等于去重模型顶点数；Mclaren 控制面板中的 Mesh Vertices 是另一种资源容量统计。
- showStats=false 只隐藏面板，当前代码仍执行计数和计时。
- 当前没有时间平滑、历史曲线、CSV 导出、每 Pass GPU Query 或自动性能回归。

## 交互边界

ImGui Backend 使用 GLFW 回调接入；滚轮由 Input 的 GLFW 回调累计。`SceneInteractionGate` 只允许活动窗口中、未被 ImGui 捕获且位于 `ViewerLayout.sceneLogical`（并排除覆盖 UI）内的按下开始拖动或消费滚轮；已开始的场景拖动可持续至松开。Input 在 attach、失焦、恢复、最小化/resize 跳帧等连续性中断时推进 epoch 并清空/抑制边缘，Gate 因而不会恢复陈旧拖动。

Mclaren 的 FreeFly 在此基础上还要求右键场景焦点：`WantCaptureKeyboard`、`WantTextInput`、鼠标捕获或窗口/场景失效会取消焦点；切换模式、重置和 epoch 改变后必须等待无按键/无鼠标按下的中性样本。这样侧栏编辑文本、UI 操作和失焦不会驱动 WASDQE。Cube 仍只使用左键 OrbitInspect 语义。当前只有单 Context，未建立多窗口 UI 管理；自由相机仍为 Mclaren 示例层功能。
