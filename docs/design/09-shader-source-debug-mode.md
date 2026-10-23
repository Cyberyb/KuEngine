# 构建与 Shader 编译设计

核对日期：2026-10-10。

源码：[根 CMake](../../CMakeLists.txt)、[Presets](../../CMakePresets.json)、[库目标](../../src/CMakeLists.txt)、[Mclaren CMake](../../examples/mclaren/CMakeLists.txt)、[Shader 脚本](../../examples/mclaren/shaders/compile_shaders.bat)。

## 目标与依赖

项目使用 CMake 3.27+、C++20，KuEngine 为静态库，公开 include 根为 src。头文件和实现一起位于 src/KuEngine，没有独立 include 目录。

KUENGINE_BUILD_EXAMPLES 和 KUENGINE_BUILD_TESTS 默认开启。App 包含 Triangle、Cube、Alpha3Pass、Mclaren、ForwardReuse、GraphResourceProbe 与 CornellBox；CTest 默认注册 core_tests 和 mclaren_camera_tests。根选项 `KUENGINE_ENABLE_GPU_SMOKE_TESTS` 默认关闭；开启后要求同时构建示例，并注册含 GraphResourceProbe 的 `gpu-smoke` 有限帧测试、受控 Validation error 负例，以及 `gpu-negative` 标签的缺 Shader 初始化失败负例。

KuEngine 库目标按 CMake 配置公开 `KU_DEBUG_BUILD`：Debug 为 `1`，其余配置为 `0`。该定义同时控制应用日志级别与 Debug 构建的 Vulkan Validation 请求；它不是 Shader 调试开关，也不改变 Shader 编译脚本的模式选择。

vcpkg 工具链提供 GLFW、ImGui、VMA、GLM、JSON、spdlog/fmt、GoogleTest 等依赖。大部分链接依赖为 PUBLIC。CMake 项目及版本宏仍为 0.1.0，历史 v0.2/v0.3 文档称谓不等同于当前发布版本号。

## 构建目录

| 入口 | 输出目录 |
|---|---|
| vs2022-vcpkg configure preset / debug build preset | build/cmake-tools/vs2022-vcpkg |
| 示例 run_*.bat | build |
| 可执行文件 | 对应构建目录/bin/Debug 或 Release |

启动时工作目录必须指向可执行文件所在目录，以找到相对 shaders/resources。两种构建目录不能混用。

## Shader 链路

RHIShader 只按显式 path/stage/entry 加载 SPIR-V，不负责运行时 GLSL 编译或热重载；Graphics 使用唯一 vertex/fragment stage，Compute 单独建立 Compute Pipeline。低层示例 CMake 在 POST_BUILD 中调用 glslc 并复制 Shader 源码/脚本；GraphResourceProbe 也复制其 `.comp` shader。Mclaren/ForwardReuse/CornellBox 依赖公共 `KuEngineForwardShaders` target，构建 forward/display 与 deferred geometry/lighting SPIR-V；Forward/Deferred 的共享 GLSL include 从资源源码目录解析。Mclaren 还复制公共 lighting.glsl、模型、JSON 和 HDR。

低层示例/旧 Mclaren POST_BUILD 的 Debug glslc 使用 -g -O0；当前公共 KuEngineForwardShaders 命令未添加这些调试标记，不以 Debug C++ 配置推断其源码调试能力。公共 Forward/Deferred 自定义 target 随依赖构建重新执行；其余 POST_BUILD Shader 变化不一定触发 App 重新链接，因此仅执行增量构建不保证 POST_BUILD 再运行；run 脚本会另行调用运行目录的 compile_shaders.bat。

## Mclaren 调试编译模式

| 模式来源 | 编译行为 |
|---|---|
| 默认非 Debug 路径 | glslc，无额外调试参数 |
| CONFIG=Debug、Debug 路径或 KU_SHADER_DEBUG=1 | glslc -g -O0 |
| KU_SHADER_SOURCE_DEBUG=1 | glslangValidator -V -gVS -Od，优先于常规模式 |

源码调试开关由 batch 脚本读取；直接 CMake 构建仍走 glslc。公共 include 优先查运行目录 shaders/common，再查源码 resources/shaders/common。

run_mclaren.bat 默认前台等待并返回应用退出码；bg 异步启动，只能反映启动动作。操作命令见 [Shader 调试指南](../usage/shader-debugging.md)。

GPU Smoke工作目录为App输出目录，SKIP_RETURN_CODE77/脚本标记明确环境skip，非GPU通过；正常matched统计/错误检查与预期负例保持。WP03-A真实Release脚本可REQUIRE_VALIDATION=OFF，默认ON，需逐真实case exit0/readback/checker通过。公共Shader输出跨配置隔离已落实，下述当前机制不重写旧失败历史。命令见 [回归检查](../usage/regression-checks.md)。

## 当前约束

WP02普通shader线性HDR/唯一Display事实延续，历史验收29项与共享证据覆写见usage历史段。WP03-A公共target新增deferred/debug.frag，直接MRT固定映射clamp后单次format encoding，无tone/exposure。examples/CMakeLists实际输出`${CMAKE_BINARY_DIR}/bin/$<CONFIG>/shaders`（forward/deferred），legacy App POST_BUILD按TARGET_FILE_DIR；本轮fresh Debug/Release并行完整构建均0，实际SPIR-V配置隔离经QA核对，不能继续把旧公共target跨配置Shader覆盖当当前事实，也不把旧失败日志改成通过。新增path_pixel_checks/run_viewer_paths，Debug完整CTest30/30显式SyncVal，Release3pass27skip另真实19 GPU groups。aggregate path/HDR/light evidence目录按`$<CONFIG>`隔离；个别legacy resize/backface prefix仍共享，Release requireValidation跳过在写入前。

没有统一 Shader 编译库、反射、热重载、Pipeline Cache 或 RenderDoc API 自动接入。源码调试信息供外部捕获工具使用，不能把这些编译标记视为自动支持所有 Shader 调试能力。

B当前永久新增`test_mclaren_scene_editing` CPU覆盖、`edit_pixel_checks`与`tests/run_mclaren_edits.cmake`/两实例多primitive-slot fixture；helper属于examples/mclaren，未改变Shader算法或Runtime状态机。B当时GPU smoke开启后CTest31项。2026-10-10独立QA fresh Debug/Release全build0，每配置core137/camera40；Debug显式SyncVal31/31(skip0/fail0)，Release3pass28skip(fail0；77非GPU通过)，另20真实Release GPU groups。B evidence为`edit-evidence/$<CONFIG>`，真实Release另用独立Release-real目录，SPIR-V仍bin/$<CONFIG>隔离。当前29samples/16edits/layout×2，命令/实际退出/历史与NOT RUN见usage；前述A/WP02计数保留为历史证据，非当前注册计数。

## CornellBox 构建与当前注册

2026-10-21 新增 [CornellBoxApp target](../../examples/cornell_box/CMakeLists.txt)，七个 App 均在 [examples/CMakeLists](../../examples/CMakeLists.txt) 注册。CornellBox 依赖 KuEngineForwardShaders，不增加自定义 shader 或复制外部素材；[启动 bat](../../examples/cornell_box/run_cornell_box.bat) 默认 Debug，也接受 Release，配置/构建 build 后在 bin/配置目录前台启动并返回退出码；它不额外运行 compile_shaders.bat。

[test_core](../../tests/CMakeLists.txt) 纳入 CornellBoxScene.cpp 和 [几何/材质测试](../../tests/core/test_cornell_box.cpp)。GPU smoke 开启时当前注册 33 项，新增 gpu_smoke_cornell_box（默认4帧）及 gpu_smoke_cornell_box_resize_recompile（6帧，1120×800，二次compile），均要求 Validation，读回输出按 cornell-evidence/$<CONFIG> 隔离。2026-10-21 独立 QA 只跑 Debug 定向7/7（0skip）及 Release core142/142，并另行启动3组真实 Release GPU；不是33项全跑。bat 仅源码核对，等价 CMake/应用命令实际通过，见 [QA](../../build/qa-cornell-box/qa-report.md)。
