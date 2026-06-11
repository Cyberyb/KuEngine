# 基础渲染、平台与示例

本主题记录 Vulkan 基线、窗口与交换链、UI、基础渲染示例及其公共架构的演进。

## 2026-04-14：Cube 示例与三维验证基线

### 背景与目标

在 Triangle 基线之上验证三维变换、鼠标交互和多种 Pipeline 状态，确认框架具备继续扩展 3D 算法示例的能力。

### 完成内容

- 新增 `CubeApp`、独立 CMake 配置、运行脚本和 Shader 编译脚本。
- 支持鼠标左键拖拽旋转、ImGui 实时调色。
- 增加实心和线框两种模式，在同一 Pass 内维护两套 Pipeline。
- 将示例接入 `examples/CMakeLists.txt`。
- 对齐核心文档与当前实现，更新项目状态和版本路线图。

### 验证

- 完成 `CubeApp` Debug 构建。
- 通过 `run_cube.bat` 完成配置、构建、Shader 编译和运行。
- 拖拽旋转、颜色调节和线框开关均生效。

### 影响与后续

- 框架已具备继续推进 3D 算法验证的基础。
- 后续需要标准化深度附件、性能观测和自动化测试。
- RenderGraph 的后续演进记录迁入 [RenderGraph 与渲染调度](render-graph-and-scheduling.md)。

## 2026-04-12：Vulkan、SwapChain 与 UI 基线

### 完成内容

- 修复交换链图像首帧布局，增加每张图像的布局跟踪。
- 修正资源销毁顺序，确保先释放 SwapChain 相关对象再销毁 Surface。
- 调整 Triangle 几何，便于直接验证输出。
- 完成 Dear ImGui 的 GLFW/Vulkan 接入。
- 增加 FPS、帧时和 Triangle 颜色调节。
- 将通用 UI 生命周期收敛到 `UIOverlay`。
- 将业务参数面板收敛到 `RenderPass::drawUI()`。
- 主循环通过 `UIOverlay` 和 `RenderPipeline` 接口驱动 UI。

### 当时形成的能力

- Vulkan 1.3 Instance、Device、Queue、SwapChain 和同步基础。
- Dynamic Rendering 最小绘制链路。
- Triangle 的 acquire、record、submit、present 完整帧流程。
- CMake、vcpkg 和一键示例脚本。

### 验证

- Triangle 示例完成构建和运行冒烟验证。
- Vulkan Validation 中与首帧布局及销毁顺序相关的关键错误被消除。

### 后续方向

- 多 Pass 调度和资源依赖进入 RenderGraph 主题。
- GPU 计时和帧时间曲线仍待建设。
