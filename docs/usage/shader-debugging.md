# Mclaren Shader 源码调试

运行脚本支持源码调试 SPIR-V。机制见 [构建与 Shader 设计](../design/09-shader-source-debug-mode.md)。

## 开启和关闭

在仓库根目录的 PowerShell 中执行：

```powershell
$env:KU_SHADER_SOURCE_DEBUG = "1"
./examples/mclaren/run_mclaren.bat Debug
```

脚本使用 glslangValidator -V -gVS -Od；需在 PATH 中能找到 Vulkan SDK 的 glslangValidator。日志应显示 source-debug-gVS。

恢复默认：

```powershell
Remove-Item Env:KU_SHADER_SOURCE_DEBUG -ErrorAction SilentlyContinue
./examples/mclaren/run_mclaren.bat Debug
```

普通符号模式可使用 KU_SHADER_DEBUG=1；Debug 配置本身已启用 glslc -g -O0。KU_SHADER_SOURCE_DEBUG 优先。

## 捕获与排查

RenderDoc 启动目标选择 build/bin/Debug/MclarenApp.exe，Working Directory 设置为同一目录。使用 preset 构建时改为对应 preset 输出目录。

先构建并执行脚本的 Shader 编译，再捕获帧查看 Shader 源码信息。仅直接运行 CMake 不读取 KU_SHADER_SOURCE_DEBUG；POST_BUILD 后再次脚本编译生成的 SPIR-V 才是最终运行版本。

缺少 lighting.glsl 时核对运行目录 shaders/common 和源码 resources/shaders/common。修改 GLSL 后调用编译脚本，不能假定只有 C++ 增量构建就一定重新生成 Shader。

M4-WP01 公共 Forward/Deferred 新共享 include 为源码 `resources/shaders/common/material_sample.glsl`、`scene_frame.glsl`、`surface_lighting.glsl`。Mclaren/ForwardReuse 依赖 `KuEngineForwardShaders` 自定义 target 编译其 SPIR-V（含 deferred geometry/lighting）；可用 `cmake --build --preset debug --target KuEngineForwardShaders` 显式重新编译，再从对应 App 输出目录启动。旧 Mclaren `lighting.glsl`/运行脚本调试链路仍属独立兼容链路；不将公共 target 的一次成功构建宣称为全项目旧 Shader 复制竞态已修复。

脚本的 bg 模式只负责异步启动；需要观察实际退出码时使用默认前台模式。

普通forward/display.frag曝光/Reinhard/sRGB机制不变；A新增deferred/debug.frag直接MRT八映射clamp后一次显示编码、不曝光/tone。公共target实际输出bin/$<CONFIG>/shaders，QA fresh Debug/Release并行完整构建和生成SPIR-V确认配置隔离；不能继续把旧公共target跨配置覆盖当当前事实或改写旧失败日志。路径正常UI切换只改静态mask不重编译shader。RenderDoc未运行。aggregate evidence按配置隔离但部分legacy读回prefix共享；真实Release GPU命令见 [回归说明](regression-checks.md)。
