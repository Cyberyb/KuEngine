# 构建与 Shader 编译设计

核对日期：2026-10-07。

源码：[根 CMake](../../CMakeLists.txt)、[Presets](../../CMakePresets.json)、[库目标](../../src/CMakeLists.txt)、[Mclaren CMake](../../examples/mclaren/CMakeLists.txt)、[Shader 脚本](../../examples/mclaren/shaders/compile_shaders.bat)。

## 目标与依赖

项目使用 CMake 3.27+、C++20，KuEngine 为静态库，公开 include 根为 src。头文件和实现一起位于 src/KuEngine，没有独立 include 目录。

KUENGINE_BUILD_EXAMPLES 和 KUENGINE_BUILD_TESTS 默认开启。App 包含 Triangle、Cube、Alpha3Pass、Mclaren、ForwardReuse 与 GraphResourceProbe；CTest 默认注册 core_tests 和 mclaren_camera_tests。根选项 `KUENGINE_ENABLE_GPU_SMOKE_TESTS` 默认关闭；开启后要求同时构建示例，并注册含 GraphResourceProbe 的 `gpu-smoke` 有限帧测试、受控 Validation error 负例，以及 `gpu-negative` 标签的缺 Shader 初始化失败负例。

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

RHIShader 只按显式 path/stage/entry 加载 SPIR-V，不负责运行时 GLSL 编译或热重载；Graphics 使用唯一 vertex/fragment stage，Compute 单独建立 Compute Pipeline。各示例 CMake 在 POST_BUILD 中调用 glslc 并复制 Shader 源码/脚本；GraphResourceProbe 也复制其 `.comp` shader，Forward Display 另有全屏 display vertex/fragment shader。Mclaren 还复制公共 lighting.glsl、模型、JSON 和 HDR。

CMake Debug 下 glslc 使用 -g -O0。Shader 变化不一定触发 App 重新链接，因此仅执行增量构建不保证 POST_BUILD 再运行；run 脚本会另行调用运行目录的 compile_shaders.bat。

## Mclaren 调试编译模式

| 模式来源 | 编译行为 |
|---|---|
| 默认非 Debug 路径 | glslc，无额外调试参数 |
| CONFIG=Debug、Debug 路径或 KU_SHADER_DEBUG=1 | glslc -g -O0 |
| KU_SHADER_SOURCE_DEBUG=1 | glslangValidator -V -gVS -Od，优先于常规模式 |

源码调试开关由 batch 脚本读取；直接 CMake 构建仍走 glslc。公共 include 优先查运行目录 shaders/common，再查源码 resources/shaders/common。

run_mclaren.bat 默认前台等待并返回应用退出码；bg 异步启动，只能反映启动动作。操作命令见 [Shader 调试指南](../usage/shader-debugging.md)。

GPU Smoke 的 CTest 工作目录为各 App 的输出目录，使用 `SKIP_RETURN_CODE 77` 或脚本标记处理环境不可用。正常 smoke 测试要求带预期计数的 `KUENGINE_COMPLETED_STATS ... status=matched`，并将 `KUENGINE_VALIDATION_ERROR`/`KUENGINE_SMOKE_FAIL` 视为失败；负例反向验证 Validation error 与初始化失败不会被误判为通过或 skip。Debug 的顺序 Shader 复制已由 QA 验证；Release 并行构建仍存在复制同名 Shader 的竞态风险。具体命令和环境条件见 [回归检查](../usage/regression-checks.md)。

## 当前约束

没有统一 Shader 编译库、反射、热重载、Pipeline Cache 或 RenderDoc API 自动接入。源码调试信息供外部捕获工具使用，不能把这些编译标记视为自动支持所有 Shader 调试能力。
