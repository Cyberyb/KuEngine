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

脚本的 bg 模式只负责异步启动；需要观察实际退出码时使用默认前台模式。
