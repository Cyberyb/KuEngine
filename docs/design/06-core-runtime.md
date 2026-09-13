# Core 与公共 Runtime 设计

核对日期：2026-09-13。

源码：[Engine.h](../../src/KuEngine/Core/Engine.h)、[Engine.cpp](../../src/KuEngine/Core/Engine.cpp)、[ApplicationRunner](../../src/KuEngine/Core/ApplicationRunner.h)、[Window](../../src/KuEngine/Core/Window.h)、[Input](../../src/KuEngine/Core/Input.h)。

## 职责与配置

Engine 聚合窗口、Vulkan 对象、渲染管线和 UI，负责创建、运行、交换链重建与销毁。四个示例均通过 `Engine::addPass<T>()`、`compile()`、`run()` 接入。

| EngineConfig 字段 | 当前含义 |
|---|---|
| title / width / height | 窗口配置；默认 1280 × 720 |
| framesInFlight | 默认且只允许 1 |
| showStats | 控制统计面板显示 |
| enableDepth | 是否创建 Runtime 深度附件，默认 false |
| depthFormat | 启用深度时，UNDEFINED 表示自动选择支持的格式 |
| clearColor / clearDepthStencil | 附件清除值 |
| depthLoadOp / depthStoreOp | 默认 CLEAR / DONT_CARE |
| depthCompareOp | 默认 LESS，传入 RenderContext |

是否启用深度由 enableDepth 决定，不能仅通过 depthFormat 是否为 UNDEFINED 判断。

`EngineRunOptions` 不属于持久渲染配置：`submittedFrameLimit=0` 保持无限交互循环；正值按成功提交的帧数停止。可选 `EngineResizeRequest` 在指定成功提交帧后调用窗口 resize，并由 `EngineRunResult` 分别记录请求与 Swapchain 重建完成。未 acquire、仅处理 resize 的循环不计入 submitted frame。该接口让自动化运行不依赖计时或强制关闭窗口，普通调用 `run()` 的行为不变。

四个示例通过 `ApplicationRunner` 解析 `--smoke-*` 参数。它在 Engine 作用域外保存 `ValidationMessageTracker`，所以 Engine 先等待 GPU、销毁 RHI/Instance 后，运行器仍可根据已捕获的 Validation error 决定最终退出码。运行器还区分参数错误（2）、普通初始化/运行失败（1）、Validation 失败（3）和已知环境不可用的 smoke skip（77）。

## 一帧时序

```mermaid
flowchart TD
    Events["Window events / Input::update"]
    Wait["等待当前 Fence"]
    Query["读取已完成命令缓冲的 GPU Timestamp"]
    Acquire["获取 SwapChain image"]
    CPU["开始 CPU render 计时"]
    UI["ImGui NewFrame / Pass update / 控制面板"]
    Begin["CommandList begin：计数清零、Query reset、起始时间戳"]
    Bind["绑定 SwapChainColor / 可选 SceneDepth"]
    Graph["Graph 节点执行：屏障、Scope、Pass draw"]
    Stats["填入当前 draw 数及最近 CPU/GPU 时间"]
    Overlay["UI Overlay Scope"]
    Final["收束外部图像布局 / 结束时间戳 / CommandList end"]
    Submit["Queue submit / 结束 CPU 计时"]
    Present["Present"]
    Decision["记录成功提交帧；可请求 resize / 达到帧上限后结束"]
    Events --> Wait --> Query --> Acquire --> CPU --> UI --> Begin
    Begin --> Bind --> Graph --> Stats --> Overlay --> Final --> Submit --> Present
    Present --> Decision
```

FrameData 携带 frameIndex、imageIndex、deltaTime、totalTime。frameIndex 是同步槽索引，单帧配置下一直为 0；imageIndex 才是获取到的交换链图像索引。

## 深度、布局与 resize

Engine 持有深度 RHITexture，尺寸跟随 SwapChain；初次创建不执行独立上传命令，首次布局转换由 Graph 完成。默认不保留深度内容；配置 LOAD 必须同时设置 STORE，否则构造时拒绝。请求 LOAD 但内容尚未初始化时，Runtime 将其转为 CLEAR。

Engine 记录每个交换链图像的布局，并按名称绑定实际 Image、View、Extent、Aspect、Layout、Load/Store、Clear 和最终布局。Present 前颜色图像转换到 PRESENT_SRC_KHR。

尺寸变化或交换链过期时，Engine 等待设备空闲、重建交换链与深度、清除外部绑定和通知 Pass::onResize。自动冒烟 resize 通过 GLFW 请求窗口尺寸变化；只有观察到新的 Swapchain generation 后才将 `resizeCompleted` 置为 true。最小化时暂缓绘制，主循环短暂休眠；M0 自动化没有覆盖人工最小化/恢复体验。

退出时先等待 GPU，再销毁 Pass、UI 和图像/命令/同步/交换链及命令池，之后依次销毁 Device、Surface、Instance 和 Window，保证依赖对象仍存活。

## Window 与 Input

Window 封装 GLFW 初始化、窗口、事件、framebuffer resize 与关闭回调；通过窗口计数控制 GLFW 初始化/终止。Input 保存全局静态按键/鼠标状态，支持 down、pressed、鼠标位置和位移；它没有滚轮查询接口，Mclaren 的滚轮值取自 ImGuiIO。

## 计时与约束

CPU 使用 Engine::Clock（当前为 high_resolution_clock）测量 acquire 成功后至 submit 返回的墙钟时间，包括 Pass 更新、UI 和命令记录；不包括事件轮询、Fence/acquire 等待与 present。详细显示语义见 [UI 与统计](05-ui-layer.md)。

Runtime 的单帧限制是命令、动态 UBO、深度和 QueryPool 复用的前提。当前没有独立场景管理器、固定时间步模拟或多窗口运行调度。
