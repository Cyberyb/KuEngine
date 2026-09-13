# 当前回归检查

本页说明检查方法，不代表已经执行过。核对日期：2026-09-13。

## 构建与 CPU 测试

前提为 VS2022 C++ 工具链、CMake 3.27+、Vulkan SDK 和仓库 vcpkg 工具链。在仓库根目录执行：

```powershell
cmake --preset vs2022-vcpkg
cmake --build --preset debug
ctest --preset debug
```

预期 core_tests 和 mclaren_camera_tests 通过。若使用已有 build 目录，则使用 `cmake --build build --config Debug` 和 `ctest --test-dir build -C Debug --output-on-failure`，不要与 preset 的输出目录混用。

## 可选 GPU Smoke

GPU Smoke 默认不注册，避免没有可用 Vulkan、窗口系统或 Debug Layer 的开发环境把普通 CPU 测试变成失败。使用独立构建目录配置：

```powershell
cmake --preset vs2022-vcpkg -B build/gpu-smoke -DKUENGINE_ENABLE_GPU_SMOKE_TESTS=ON
cmake --build build/gpu-smoke --config Debug
ctest --test-dir build/gpu-smoke -C Debug -L gpu-smoke --output-on-failure
ctest --test-dir build/gpu-smoke -C Debug -L gpu-negative --output-on-failure
```

`gpu-smoke` 运行四个示例的有限帧路径；Triangle 还在第 2 个成功提交帧后请求 960 × 540 resize。`gpu_smoke_validation_injection` 是受控负例：应用必须报告 Validation failure，CTest 才视为通过。`gpu-negative` 的缺 Shader 负例要求业务初始化失败，不能误报为环境 skip。

| 结果 | 应用退出码/标记 | 含义 |
|---|---|---|
| PASS | 0 / `KUENGINE_SMOKE_PASS` | 达到有限提交帧与需要的 resize，且已捕获 Validation error 为 0 |
| FAIL | 1 或 `KUENGINE_SMOKE_FAIL` | 初始化、运行、帧数、resize 或其他业务失败 |
| Validation FAIL | 3 / `KUENGINE_SMOKE_FAIL reason=validation` | Messenger 捕获到至少一个 error；`KUENGINE_VALIDATION_ERROR` 是日志标记 |
| SKIP | 77 / `KUENGINE_SMOKE_SKIP` | Vulkan/Surface/设备不可用，或 `--smoke-require-validation` 但 Release、Layer、Debug Utils 不满足；不是通过 |

可从 App 输出目录手动运行，例如：

```powershell
.\TriangleApp.exe --smoke-frames 5 --smoke-require-validation --smoke-resize-after 2 960 540
```

`--smoke-frames 0` 保持无限运行；`--smoke-inject-validation-error` 只用于受控负例，必须与 `--smoke-require-validation` 和正帧数一起使用。未知参数或不安全的 resize/injection 组合返回 2。

## 示例冒烟

从对应 exe 所在目录启动，并保持多帧运行，检查首帧、UI、resize、最小化和恢复。

| 示例 | 关键检查 | 业务计数预期 |
|---|---|---|
| Triangle | 颜色调整，稳定输出 | 1 draw / 3 vertices |
| Cube | 拖拽、实心/线框切换 | 1 / 36 或 1 / 24 |
| Alpha3Pass | 三形状顺序、颜色混合、后续节点保留前一节点颜色 | 全部启用时 3 / 9 |
| Mclaren | 材质、Skybox、视口、深度与相机 | 非空 SubMesh 的 draw 加可选 Skybox；关闭 Skybox 减 1 draw、3 vertices |

计数只在 Pipeline/资源就绪且实际记录绘制时成立，不包含 ImGui 内部 draw。模型顶点容量不同于索引提交数量。

## Graph 与时间统计

Graph Debug 面板应显示编译依赖和屏障计划，执行摘要包含已应用屏障、资源转换及 Rendering Scope。当前已不使用旧 skipped-in-rendering 开关作为正常执行路径。

CPU/GPU 首帧可为 N/A；有 Timestamp 能力时经过提交和 Fence 等待后应能读到 GPU 时间。不支持的设备显示 N/A。改变业务绘制负载后观察趋势，避免将 Present/VSync 的整帧限制误解为纯 GPU Shader 时间。

Debug 构建会请求 Validation Layer，并先检查 `VK_LAYER_KHRONOS_validation` 和 `VK_EXT_debug_utils` 是否可用。控制台的 `Vulkan instance created` 行会给出 requested、enabled 和 message capture 三个状态；缺少 Layer 或 Debug Utils 时会记录警告。Messenger 可用时，Vulkan general/validation/performance 消息以 `Vulkan <type> [<message-id>]` 写入 logger，error 还带有 `KUENGINE_VALIDATION_ERROR` 标记。

ApplicationRunner 会在 Engine 销毁后检查共享 Tracker，并把已捕获的 error 转为退出码 3；`--smoke-require-validation` 额外要求消息捕获能力可用。窗口存活或 Smoke PASS 只说明启动、有限提交、退出、指定 resize 和已捕获错误检查通过，不能替代人工画面、输入、最小化/恢复或像素正确性检查。
