# 当前回归检查

本页说明检查方法，不代表已经执行过。核对日期：2026-09-12。

## 构建与 CPU 测试

前提为 VS2022 C++ 工具链、CMake 3.27+、Vulkan SDK 和仓库 vcpkg 工具链。在仓库根目录执行：

```powershell
cmake --preset vs2022-vcpkg
cmake --build --preset debug
ctest --preset debug
```

预期 core_tests 和 mclaren_camera_tests 通过。若使用已有 build 目录，则使用 `cmake --build build --config Debug` 和 `ctest --test-dir build -C Debug --output-on-failure`，不要与 preset 的输出目录混用。

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

Debug 请求 Validation Layer。当前未实现统一 Debug Messenger/自动错误失败机制，人工运行应收集控制台或验证工具输出，检查 VUID、布局、Query 和资源生命周期错误。窗口存活只能说明冒烟通过，不能替代图像正确性检查。
