# Triangle 示例技术说明

本文档解释 `examples/triangle` 示例中实际使用的关键技术与执行流程。

## 1. 技术栈

- 图形 API: Vulkan 1.3
- 渲染方式: Dynamic Rendering（`vkCmdBeginRendering` / `vkCmdEndRendering`）
- 窗口系统: GLFW3
- 语言标准: C++20
- 构建系统: CMake + vcpkg
- Shader 工具: `glslc`（来自 Vulkan SDK）

## 2. 运行时模块关系

示例主程序在 [examples/triangle/main.cpp](examples/triangle/main.cpp) 中直接串联以下模块：

- `Window`: GLFW 窗口与事件循环
- `RHIInstance`: Vulkan 实例创建
- `RHIDevice`: 物理设备选择、逻辑设备、队列、VMA
- `SwapChain`: 交换链图像与视图管理
- `SyncManager`: 每帧信号量与 Fence 同步
- `CommandList`: 命令缓冲录制
- `RenderPipeline + TrianglePass`: 渲染通道组织与三角形 draw call

## 3. 图形管线要点

实现位于 [src/KuEngine/RHI/RHIPipeline.cpp](src/KuEngine/RHI/RHIPipeline.cpp)。

- 使用 2 个 shader stage：Vertex + Fragment
- 无顶点缓冲输入（全屏三角形由 `gl_VertexIndex` 生成）
- 开启动态状态：Viewport / Scissor
- 通过 `VkPipelineRenderingCreateInfo` 绑定 color attachment format（无需传统 `VkRenderPass`）

## 4. 帧渲染流程

主循环每帧执行流程如下：

1. `waitForFrame` 等待上一帧 GPU 完成
2. `acquireNextImage` 获取交换链图像索引
3. 录制命令缓冲
4. 图像布局转换：`PRESENT_SRC_KHR -> COLOR_ATTACHMENT_OPTIMAL`
5. `vkCmdBeginRendering`
6. 执行 `TrianglePass::execute`，内部调用 `vkCmdDraw(cmd, 3, 1, 0, 0)`
7. `vkCmdEndRendering`
8. 图像布局转换：`COLOR_ATTACHMENT_OPTIMAL -> PRESENT_SRC_KHR`
9. 提交队列并 present

## 5. Shader 技术点

Shader 源码位置：

- [examples/triangle/shaders/triangle.vert](examples/triangle/shaders/triangle.vert)
- [examples/triangle/shaders/triangle.frag](examples/triangle/shaders/triangle.frag)

关键设计：

- 顶点着色器不依赖顶点缓冲，直接构造覆盖屏幕的三角形坐标
- 片段着色器输出固定颜色，便于验证渲染链路是否打通

## 6. CMake 集成点

在 [examples/triangle/CMakeLists.txt](examples/triangle/CMakeLists.txt) 中：

- 生成 `TriangleApp` 可执行文件
- 构建后复制 shader 源文件与脚本到运行目录
- 若检测到 `Vulkan_GLSLC_EXECUTABLE`，自动编译 `.vert/.frag` 为 `.spv`

## 7. 当前实现边界

- 该示例目标是“最小可运行三角形链路”，优先验证 RHI 与渲染流程
- 尚未接入完整 UI、资源系统、复杂材质与多 Pass 调度
- 可作为后续扩展（纹理、uniform、相机、RenderGraph）的基线示例
