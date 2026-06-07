# Shader 源码调试编译模式设计说明（RenderDoc）

## 1. 背景与目标

在 Vulkan 渲染链路中，GPU 实际执行的是 SPIR-V，而不是 GLSL 源码。RenderDoc 的 shader 调试也基于 SPIR-V 指令级执行。

KuEngine 新增“源码调试编译模式”的目标是：
- 在调试阶段尽可能保留 GLSL 源码映射信息，提升 RenderDoc 中的可读性与可定位性。
- 保持默认构建流程不变，不影响常规运行与发布配置。
- 通过环境变量实现快速开/关，无需修改工程代码。

---

## 2. 实现范围

当前该能力接入在 Mclaren 示例链路：
- 编译脚本：`examples/mclaren/shaders/compile_shaders.bat`
- 一键运行：`examples/mclaren/run_mclaren.bat`
- 示例 shader：`examples/mclaren/shaders/mclaren.vert`、`examples/mclaren/shaders/mclaren.frag`
- 公共片段：`resources/shaders/common/lighting.glsl`

说明：
- `run_mclaren.bat` 在构建后会进入运行目录 `build/bin/<Config>/shaders` 并调用 `compile_shaders.bat`。
- 运行目录中的脚本和 shader 文件来自 CMake 的 POST_BUILD 拷贝。

---

## 3. 核心机制

### 3.1 两种编译后端

`compile_shaders.bat` 支持两条编译路径：

1. 默认路径（常规调试符号）
- 编译器：`glslc`
- 调试参数：`-g -O0`（Debug 配置下）
- 目的：保留基础调试信息，流程简单稳定。

2. 源码调试路径（RenderDoc 友好）
- 编译器：`glslangValidator`
- 调试参数：`-gVS -Od`
- 目的：生成更偏向源码调试的 SPIR-V 信息，改善 RenderDoc 源码映射体验。

### 3.2 环境变量开关

脚本支持以下环境变量：

- `KU_SHADER_SOURCE_DEBUG`
  - `1`：启用源码调试路径（`glslangValidator -gVS -Od`）。
  - 其他值或未设置：不启用。

- `KU_SHADER_DEBUG`
  - `1`：强制默认路径使用 `-g -O0`。
  - 该变量仅影响 `glslc` 路径，不切换到 `glslangValidator`。

- `CONFIG`
  - 若值为 `Debug`，默认开启 `glslc -g -O0`（即使未设置 `KU_SHADER_DEBUG`）。

### 3.3 模式优先级

当前脚本逻辑优先级为：

1. 若 `KU_SHADER_SOURCE_DEBUG=1`：进入 `source-debug-gVS` 模式。
2. 否则，若 `CONFIG=Debug` 或 `KU_SHADER_DEBUG=1`：进入 `debug-symbols` 模式（`glslc -g -O0`）。
3. 否则：`optimized-no-debug`。

脚本输出会打印：
- `Shader compile mode: ...`
- `Shader debug flags: ...`（如有）
- `Shader source debug compiler: glslangValidator -gVS -Od`（源码调试模式下）

---

## 4. include 与公共 shader 片段

`mclaren.frag` 使用：

```glsl
#extension GL_GOOGLE_include_directive : require
#include "lighting.glsl"
```

为保证在源码目录与运行目录都可工作，脚本会自动解析 common 目录并传入 include 路径：
- `LOCAL_COMMON_DIR`：`<shader_dir>/common`
- `REPO_COMMON_DIR`：`<shader_dir>/../../../resources/shaders/common`
- `REPO_COMMON_DIR_ALT`：`<shader_dir>/../../../../resources/shaders/common`

若三处都不存在 `lighting.glsl`，脚本会报错并退出。

---

## 5. 用户操作指南（开启/关闭）

### 5.1 PowerShell（推荐）

启用源码调试模式并运行：

```powershell
$env:KU_SHADER_SOURCE_DEBUG = "1"
./examples/mclaren/run_mclaren.bat
```

关闭源码调试模式（恢复默认）：

```powershell
Remove-Item Env:KU_SHADER_SOURCE_DEBUG -ErrorAction SilentlyContinue
./examples/mclaren/run_mclaren.bat
```

也可显式关闭：

```powershell
$env:KU_SHADER_SOURCE_DEBUG = "0"
./examples/mclaren/run_mclaren.bat
```

### 5.2 Windows CMD

启用源码调试模式：

```bat
set KU_SHADER_SOURCE_DEBUG=1
examples\mclaren\run_mclaren.bat
```

关闭源码调试模式：

```bat
set KU_SHADER_SOURCE_DEBUG=
examples\mclaren\run_mclaren.bat
```

---

## 6. RenderDoc 调试建议流程

1. 使用源码调试模式重编译 shader（见第 5 节）。
2. 启动 `MclarenApp`，使用 RenderDoc 捕获目标帧。
3. 打开目标 drawcall 的 Fragment Shader 调试视图。
4. 关注是否能映射到 GLSL 源文件/行号（而非仅裸 SPIR-V 指令）。
5. 结合 UI 参数（光照方向/颜色/强度、normal/ORM 开关）做逐步定位。

说明：
- Vulkan 下仍是 SPIR-V 执行模型；“源码调试”是通过调试信息映射实现，不是直接执行 GLSL。

---

## 7. 自检与验证命令

检查当前模式日志：

```powershell
cmd /c .\examples\mclaren\run_mclaren.bat 2>&1 |
  findstr /i /c:"Shader compile mode" /c:"Shader debug flags" /c:"Shader source debug compiler"
```

检查 SPIR-V 是否包含行信息（`OpLine`）：

```powershell
$spv = "build/bin/Debug/shaders/mclaren.frag.spv"
$asm = "build/bin/Debug/shaders/mclaren.frag.spvasm"
spirv-dis $spv -o $asm
(Select-String -Path $asm -SimpleMatch "OpLine" | Measure-Object).Count
```

---

## 8. 常见问题排查

1. 日志显示 `optimized-no-debug`
- 原因：未设置调试变量且未在 Debug 场景触发。
- 处理：设置 `KU_SHADER_SOURCE_DEBUG=1`，重新运行脚本。

2. include 报错找不到 `lighting.glsl`
- 原因：common 目录未复制或路径不匹配。
- 处理：确认 `resources/shaders/common/lighting.glsl` 存在，并重新执行 `run_mclaren.bat`。

3. RenderDoc 里仍看起来像纯 SPIR-V
- 先确认脚本输出模式为 `source-debug-gVS`。
- 再确认当前抓帧使用的是刚编译出的 `build/bin/Debug/shaders/mclaren.frag.spv`。
- 使用第 7 节命令检查 `OpLine` 数量是否大于 0。

4. 找不到 `glslangValidator`
- 原因：Vulkan SDK 未安装或 PATH 未生效。
- 处理：安装 Vulkan SDK，重开终端后执行 `where glslangValidator` 验证。

---

## 9. 与 CMake 构建链路关系

`examples/mclaren/CMakeLists.txt` 在 Debug 配置下会为 `glslc` 增加 `-g -O0`，用于保证常规 Debug 构建也带基础符号。

一键运行脚本中的第 3 步（`compile_shaders.bat`）会再次编译 shader，并可按环境变量切换为源码调试模式。

因此，推荐调试实践是：
- 用 `run_mclaren.bat` 触发最终可执行与 shader 编译。
- 通过环境变量决定“常规调试符号”或“源码调试模式”。

---

## 10. 运行模式：前台 vs 后台

`examples/mclaren/run_mclaren.bat` 当前支持两种运行模式：

1. 前台模式（默认）
- 行为：直接执行 `MclarenApp.exe`，脚本会阻塞等待程序结束。
- 优点：可获得应用真实退出码，便于定位“启动后立即退出/崩溃”。
- 适用：调试、回归验证、CI 脚本化检查。

2. 后台模式（`bg`）
- 行为：通过 `start "MclarenApp" "MclarenApp.exe"` 异步启动。
- 优点：脚本立即返回，不阻塞终端。
- 限制：脚本层面通常拿不到应用进程真实退出码。
- 适用：日常快速拉起应用、并行操作终端。

### 10.1 参数规则

脚本参数解析如下：

- `run_mclaren.bat`
  - `CONFIG=Debug`
  - `RUN_MODE=foreground`

- `run_mclaren.bat bg`
  - `RUN_MODE=background`
  - `CONFIG` 保持默认 `Debug`

- `run_mclaren.bat <Config>`
  - 设置 `CONFIG=<Config>`
  - `RUN_MODE=foreground`

- `run_mclaren.bat <Config> bg`
  - 设置 `CONFIG=<Config>`
  - `RUN_MODE=background`

### 10.2 退出码语义

- 前台模式：
  - `MclarenApp.exe` 的退出码会回传给脚本。
  - 非零退出码会触发：`MclarenApp exited with code <N>.`

- 后台模式：
  - 脚本仅表示“启动动作成功”，通常返回 0。
  - 不能据此判断应用是否在运行中崩溃。

### 10.3 推荐实践

- 需要确认稳定性或排查闪退：优先使用前台模式。
- 仅需快速启动窗口：使用后台模式。
