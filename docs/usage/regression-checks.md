# 当前回归检查

本页说明检查方法与已验收的 M0、M1 范围。核对日期：2026-09-14。

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

`gpu-smoke` 运行四个示例的有限帧路径；Triangle 还在第 2 个成功提交帧后请求 960 × 540 resize。它要求 `KUENGINE_COMPLETED_STATS` 中的实际业务计数与 Pass 预期值匹配：Triangle 1/3、默认实心 Cube 1/36、Alpha3Pass 3/9；Mclaren 使用非零动态公式。`gpu_smoke_validation_injection` 是受控负例：应用必须报告 Validation failure，CTest 才视为通过。`gpu-negative` 的缺 Shader 负例要求业务初始化失败，不能误报为环境 skip。

| 结果 | 应用退出码/标记 | 含义 |
|---|---|---|
| PASS | 0 / `KUENGINE_SMOKE_PASS` | 达到有限提交帧与需要的 resize，且已捕获 Validation error 为 0 |
| FAIL | 1 或 `KUENGINE_SMOKE_FAIL` | 初始化、运行、帧数、resize 或其他业务失败 |
| Validation FAIL | 3 / `KUENGINE_SMOKE_FAIL reason=validation` | Messenger 捕获到至少一个 error；`KUENGINE_VALIDATION_ERROR` 是日志标记 |
| SKIP | 77 / `KUENGINE_SMOKE_SKIP` | Vulkan/Surface/设备不可用，或 `--smoke-require-validation` 但 Release、Layer、Debug Utils 不满足；不是通过 |

`KUENGINE_COMPLETED_STATS` 同时输出 completed submit 编号、draws、vertices、CPU 毫秒、GPU 状态（`unsupported` / `waiting` / `available`）以及预期值。只有 `available` 才有 GPU 毫秒；实体不支持 Timestamp 的 GPU 尚未在 M0 验证，不能将当前 RTX 4060 Ti 的结果外推到该情况。

可从 App 输出目录手动运行，例如：

```powershell
.\TriangleApp.exe --smoke-frames 5 --smoke-require-validation --smoke-resize-after 2 960 540
```

`--smoke-frames 0` 保持无限运行；`--smoke-inject-validation-error` 只用于受控负例，必须与 `--smoke-require-validation` 和正帧数一起使用。未知参数或不安全的 resize/injection 组合返回 2。

可额外运行收起侧栏冒烟，确认 Runner 输出的 `KUENGINE_VIEWER_LAYOUT first/final` 合法且场景像素矩形没有溢出：

```powershell
.\TriangleApp.exe --smoke-frames 5 --smoke-require-validation --smoke-sidebar-collapsed
```

该开关只检查紧凑模式下的自动化布局契约，不能代替人工侧栏交互检查。

## 示例冒烟

从对应 exe 所在目录启动，并保持多帧运行，检查首帧、UI、resize、最小化和恢复。

| 示例 | 关键检查 | 业务计数预期 |
|---|---|---|
| Triangle | 颜色调整，稳定输出 | 1 draw / 3 vertices |
| Cube | 唯一右侧栏下左键场景拖拽、场景滚轮缩放、UI/侧栏不改变相机，实心/线框切换；最小化/恢复后再检查 | 1 / 36 或 1 / 24 |
| Alpha3Pass | 三形状顺序、颜色混合、后续节点保留前一节点颜色；展开 Parameters 中三个 Pass 的同名控件相互独立 | 全部启用时 3 / 9 |
| Mclaren | 侧栏中的 Scene/材质/相机/环境分组、Skybox、深度与共享场景 viewport；Orbit 左键/滚轮，FreeFly 模式、右键场景焦点、文本/UI 隔离与 reset | 非空 SubMesh 的 draw 加可选 Skybox；关闭 Skybox 减 1 draw、3 vertices |

计数只在 Pipeline/资源就绪且实际记录绘制时成立，不包含 ImGui 内部 draw。模型顶点容量不同于索引提交数量。QA 已在 RTX 4060 Ti Debug 环境确认四例唯一侧栏、收起/紧凑/隐藏/重开、小窗口滚动、Alpha 控件隔离、Mclaren 分组、展开/收起比例与无黑缝；也确认 Cube 滚轮/UI 隔离、最小化/恢复、resize 和 Validation 检查。Mclaren 还确认 Orbit/FreeFly 两向切换无跳、文本输入不移动、reset、收展和最大化/最小化。这不是持续右键拖动、WASDQE 六向/组合 held 手势的可靠实机记录，也不是 Win+D、物理非等比 DPI 或截图/像素参考比对。

## 侧栏与场景输入人工检查

1. 分别启动四例，确认只有右侧 `KuEngine Sidebar`；展开时检查 Performance、Parameters / Scene、Render Graph，缩小窗口后确认可垂直滚动。
2. 点击 Collapse Sidebar，确认紧凑 Performance 覆盖场景而场景恢复完整宽度；点击 Hide compact stats 后仍能用 Open Sidebar 重开。展开/收起期间场景比例正确，右侧不留黑缝。
3. 在 Cube 与 Mclaren 中，从场景区域按下左键后可持续旋转至松开；从侧栏或被 UI 捕获的位置按下不得在进入场景后开始旋转。滚轮只在场景内缩放。
4. resize、最小化并恢复后重复第 2、3 步。已验收的人工范围不包括 Win+D、物理非等比 DPI 或像素结果对比；这些项目不能标作通过。

## Mclaren 双模式相机检查

1. 在 Camera 的 `Mode` 中切换 Orbit Inspect / Free Fly，确认切换前后画面没有明显跳变；切换后先释放鼠标和 WASDQE，再开始下一次输入。
2. Orbit 模式确认左键场景拖动旋转模型、滚轮缩放；Free Fly 先从场景区域右键获得焦点，再检查 `W/S/A/D`、`Q/E`、右键视角、Move Speed 和两个 Reset 按钮的预期语义。
3. 点击/编辑侧栏输入框、窗口失焦或恢复后，确认没有移动或残留输入；折叠/重开和最大化/最小化后再检查模式、统计和 Validation 输出。
4. 已验收的实机记录覆盖双向切换、文本不移动、reset、侧栏与窗口状态；持续右键拖动、WASDQE 六向和组合 held 手势仅有永久测试/静态覆盖，仍应作为后续人工补测项。

## Graph 与时间统计

Graph Debug 面板应显示编译依赖和屏障计划，执行摘要包含已应用屏障、资源转换及 Rendering Scope。当前已不使用旧 skipped-in-rendering 开关作为正常执行路径。

FPS/Frame 是当前主循环样本；CPU/GPU/Draw/Vertices 是同一 completed submit。首个完成提交前显示 waiting/N/A；GPU 分为 Unsupported、Waiting、Available，有 Timestamp 且完成后才显示 GPU 时间。改变业务绘制负载后观察趋势，避免将 Present/VSync 的整帧限制误解为纯 GPU Shader 时间。

Debug 构建会请求 Validation Layer，并先检查 `VK_LAYER_KHRONOS_validation` 和 `VK_EXT_debug_utils` 是否可用。控制台的 `Vulkan instance created` 行会给出 requested、enabled 和 message capture 三个状态；缺少 Layer 或 Debug Utils 时会记录警告。Messenger 可用时，Vulkan general/validation/performance 消息以 `Vulkan <type> [<message-id>]` 写入 logger，error 还带有 `KUENGINE_VALIDATION_ERROR` 标记。

ApplicationRunner 会在 Engine 销毁后检查共享 Tracker，并把已捕获的 error 转为退出码 3；`--smoke-require-validation` 额外要求消息捕获能力可用。窗口存活或 Smoke PASS 只说明启动、有限提交、退出、指定 resize 和已捕获错误检查通过，不能替代人工画面、输入、最小化/恢复或像素正确性检查。

QA 对 M1-WP03 的自动证据包括 Debug/Release CPU 测试、Debug GPU Smoke 7/7，以及 Release 与缺 Validation Layer 的 skip/失败语义检查；这些不等于完整的连续人机输入覆盖。

当前残余风险：Release 并行构建仍可能竞争复制同名 Shader；部分同步 wait 的 Vulkan 返回值尚未统一检查。两项均不在 M0 中宣称解决。
