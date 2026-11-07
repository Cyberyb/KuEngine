# 开发与运行

按需阅读：[构建](#build) · [启动与参数](#run) · [Shader/资源](#shader) · [新增特性](#feature) · [验证](#verification)。示例专用操作见[示例手册](examples.md)，模块契约见[当前架构](../design/README.md)。

<a id="build"></a>
## 环境与构建

当前主要验证环境为 Windows 10/11、VS2022 C++ 工具链、CMake 3.27+、Vulkan SDK 1.3+ 与仓库 vcpkg；C++20。Vulkan 设备须满足当前 1.3/Dynamic Rendering/synchronization2 基线。默认两例GPU不要求Python3，显式扩展的体积专项才需要。

在仓库根目录执行；第一次 configure 会通过 vcpkg 工具链解析依赖：

```powershell
cmake --preset vs2022-vcpkg
cmake --build --preset debug --target CornellBoxApp
Push-Location build/cmake-tools/vs2022-vcpkg/bin/Debug
try { ./CornellBoxApp.exe } finally { Pop-Location }
```

CornellBox 不需要外部模型/HDR，适合首次启动。换目标名即可构建其他[示例](examples.md)。Release 使用 `cmake --build --preset release`，运行目录改为 bin/Release。构建所有目标可省略 --target。

| 入口 | 构建目录 | 注意 |
|---|---|---|
| CMake preset | build/cmake-tools/vs2022-vcpkg | 本手册默认入口 |
| 既有 run_*.bat | build | 运行 exe 在 build/bin/Debug 或 Release |
| VolumetricIntegration bat | 默认同 preset | 可用 KUENGINE_BUILD_DIR 覆盖 |

不要混用不同构建目录的 exe、SPIR-V、资源和测试日志。手动运行必须先进入 exe 所在目录。bat 前台模式返回真实应用退出码；bg 模式只能表示已发起启动。

<a id="run"></a>
## 公共运行参数

下面参数传给示例 exe，不是 CMake 参数：

| 参数 | 行为 |
|---|---|
| --frames-in-flight 1 或 2 | 帧槽数，默认2，0/3拒绝 |
| --smoke-frames N | 实际成功提交 N 帧后退出；0持续交互 |
| --smoke-resize-after FRAME WIDTH HEIGHT | 指定提交后 resize；帧上限至少再留两帧 |
| --smoke-recompile-after-setup | 在 idle 边界执行第二次 compile |
| --smoke-require-validation | 要求可捕获 Validation，环境不可用退出77 |
| --smoke-sidebar-collapsed | 收起侧栏启动 |
| --smoke-inject-validation-error | 仅用于受控负例，检查错误捕获和非零退出 |

Mclaren/ForwardReuse/CornellBox 另支持 `--render-path forward|deferred`，`--deferred` 为兼容别名。CLI 优先于 scene JSON；不同选择冲突、非法/缺值报错，CLI 不绕过坏配置。无 Deferred 能力时显式请求失败，UI 给出原因。

正常有限帧退出应核对 actual exit、完成提交与预期统计；参数错误2、初始化失败1、捕获 Validation error3。77是环境跳过，不是 GPU 通过。侧栏的 Performance/Parameters/Render Graph 是公共入口；相机操作因示例而异。

<a id="shader"></a>
## Shader、资源与调试

RHI 只加载 SPIR-V，不运行时编译或热重载。glslc 应在 Vulkan SDK/工具链可发现范围内；缺工具时先修环境，再 configure/build。

| Shader 路径 | 构建机制 |
|---|---|
| Triangle/Cube/Alpha3Pass/Probe | App POST_BUILD 调 glslc；仅改 GLSL 不一定触发 App relink |
| 公共 Forward/Deferred | KuEngineForwardShaders target，输出 bin/配置/shaders/forward 与 deferred |
| VolumetricIntegration | 每文件 OUTPUT+DEPENDS，独立 shaders/volumetric_integration，Debug为 -g -O0 |

旧例 Shader-only 修改后需实际调用其编译脚本或重触发 App 构建，检查 SPIR-V 时间戳。公共 target 当前未给 glslc 加源码调试标记，不能凭 C++ Debug 推断已生成调试符号。

Mclaren 旧脚本支持 KU_SHADER_DEBUG=1（glslc -g -O0）和更高优先级 KU_SHADER_SOURCE_DEBUG=1（glslangValidator -V -gVS -Od）。这些开关只影响脚本路径，不能替代当前公共 Forward/Deferred target 的调试配置。使用外部捕获工具前确认实际加载的 SPIR-V。

```powershell
$env:KU_SHADER_SOURCE_DEBUG='1'
./examples/mclaren/run_mclaren.bat Debug
Remove-Item Env:KU_SHADER_SOURCE_DEBUG -ErrorAction SilentlyContinue
```

捕获工具 Working Directory 与 exe 目录一致。共享 GLSL 在 resources/shaders/common；找不到 include 时检查源码与运行目录副本。这里给出操作方法，不声称本次执行了 RenderDoc。

资源先查运行目录 resources，再按示例的源码回退路径查找；JSON 源文件改动后需重新复制/构建。Scene/Material 字段以[AssetConfig](../../src/KuEngine/Asset/AssetConfig.h)与[资产契约](../design/README.md#assets)为准，Registry 不会自动发现资源。

<a id="feature"></a>
## 新增 Graphics、Compute 或 native 特性

推荐从 [Probe main](../../examples/graph_resource_probe/main.cpp) 或 [Volumetric main](../../examples/volumetric_integration/main.cpp) 复制小型装配方式，保留公共 Runtime：

1. 在 runApplication 的装配回调中 addPass 或 addCallbackPass。
2. initialize/prepare 创建 Shader、Pipeline、参数与 sampler，由 Pass/参数对象持有。
3. setup/declare 用 create/import 声明资源、use/attachment 及 dependsOn。
4. execute 按准确 use/range resolve 当前物理资源，再记录算法命令。
5. 资产/实例/路径等结构变更先排队，低频共享 GPU 对象替换走安全等待；普通标量参数可当前 UI 更新，为新增路径补验证。

对象 Graphics 实现 initialize、setup(builder)、execute(commands,frame,resolver)，按需实现 update/drawUI/onResize。Callback 使用 CallbackPassDesc：declare/prepare 可修改 Parameters，execute 只读 Parameters。不要在每次 execute 随意创建配置相关 GPU 对象。

Compute prepare 先检查 graphicsQueueSupportsCompute；显式 COMPUTE stage/storage binding，execute 调 bindComputePipeline/bindParameterSet/dispatch。Compute/Transfer 必须在 attachment scope 外，当前同 Graphics Queue，不是 Async Compute。

native callback 声明 NativeCommandCapability/SideEffect，execute 请求子集 scope，再用 checked resolve 取得资源。scope/command buffer 仅执行期借用；不偷偷另行提交，不把布局变为 Graph 不知道的状态。旧对象 Pass 属 TrustedLegacyObject，裸命令仍需人工审查与 Validation。

内部资源归 Graph pool；同名 descriptor 必须完全一致，注册顺序影响依赖方向。resize/recompile 后刷新 handles，allocationGeneration/ImageView 改变后重绑 descriptor。普通跨 Pass barrier 由 Graph 记录。

外部资源先 import，录制前 bind，传初始/最终状态和 contentsValid；拥有者保证 GPU 寿命和图外同步。export 须先声明、成功 execute 后取 token 并检查 valid；它不是跨帧 retire handle。资源、导出与 native 的完整边界见[Graph 契约](../design/README.md#graph)。

Probe 的参考链：ImageClear → 固定 BufferFill → native copy128B → Compute copy32uint → sampled indexed-indirect draw → PostDrawCopy。应出现深色背景蓝三角和1draw/3提交索引，不把固定 copy 示例当作复杂 GPU 生成几何验证。

新增 ShadingModel 同步 CPU 枚举/严格配置、材质解析与 per-draw 编码、shared GLSL、Forward/Deferred 分派，并明确 GBuffer 表达或 Forward-only 限制，不修改中央 Runtime 绘制循环。

<a id="verification"></a>
## 验证方法与证据边界

默认范围遵循 [AGENTS测试约定](../../AGENTS.md#testing)：新增功能/特性只实际运行 MclarenApp 与 CornellBoxApp，相关 CPU 单元/契约测试继续执行。其他六例、内部启动其他例的混合矩阵与全量 GPU 仅在用户或当前任务包明确要求时运行；阅读 Probe 接入源码不等于默认启动 Probe。

### CPU检查与注册开关

CPU 测试不启动示例，可在仓库根目录运行：

```powershell
cmake --preset vs2022-vcpkg
cmake --build --preset debug --target test_core test_mclaren_camera test_volumetric_options
ctest --preset debug -R '^(core_tests|mclaren_camera_tests|volumetric_options)$' --output-on-failure
```

以下是配置/注册范围，不是已经执行的结果（具体名称以当前 CMake/CTest 清单为准）：

| 配置 | 注册内容 | 依赖与限制 |
|---|---|---|
| GPU_SMOKE=OFF、EXTENDED=OFF | 3项CPU | GPU总开关初始默认OFF |
| GPU_SMOKE=ON、EXTENDED=OFF | 3项CPU+13项两例GPU，共16 | 默认功能回归，要求examples=ON，不要求Python |
| GPU_SMOKE=ON、EXTENDED=ON | 恢复原40项清单 | 显式扩展，含其他例/混合矩阵；体积专项要求Python3 |

表中开关全名分别为 KUENGINE_ENABLE_GPU_SMOKE_TESTS 与 KUENGINE_ENABLE_EXTENDED_GPU_TESTS。EXTENDED 初始默认OFF，普通 configure preset也显式重置OFF；GPU总开关沿用已有cache值，需纯CPU清单时显式设GPU_SMOKE=OFF。EXTENDED=ON而GPU_SMOKE=OFF会配置失败。

### 默认两例的真实回归

```powershell
cmake --preset vs2022-vcpkg -DKUENGINE_ENABLE_GPU_SMOKE_TESTS=ON -DKUENGINE_ENABLE_EXTENDED_GPU_TESTS=OFF
cmake --build --preset debug --target test_core test_mclaren_camera test_volumetric_options MclarenApp CornellBoxApp edit_pixel_checks
ctest --preset debug -N
```

-N只列清单；应看到16项，再用串行CTest真实运行。保存并恢复原SyncVal环境，避免同时写相同读回证据：

```powershell
$taskHadEnables=Test-Path Env:VK_LAYER_ENABLES
$taskPriorEnables=$env:VK_LAYER_ENABLES
try {
    $env:VK_LAYER_ENABLES='VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT'
    ctest --preset debug -j 1 --output-on-failure
    "ACTUAL_CTEST_EXIT=$LASTEXITCODE"
} finally {
    if ($taskHadEnables) { $env:VK_LAYER_ENABLES=$taskPriorEnables }
    else { Remove-Item Env:VK_LAYER_ENABLES -ErrorAction SilentlyContinue }
}
```

仅在EXTENDED=OFF的清单上执行以上无-R命令，确保不启动其他例。构建更多target不代表启动或验收那些例。CPU-only/77skip不能代替两例真实GPU回归。

### 明确要求后的专项扩展

只有当前用户/任务包已允许某专项时才开启扩展，并用-R只选该专项。例如明确要求Probe资源检查后：

```powershell
cmake --preset vs2022-vcpkg -DKUENGINE_ENABLE_GPU_SMOKE_TESTS=ON -DKUENGINE_ENABLE_EXTENDED_GPU_TESTS=ON
cmake --build --preset debug --target GraphResourceProbeApp
ctest --preset debug -N -R '^gpu_smoke_graph_resource_probe(_recompile)?$'
```

确认清单后，在上面的SyncVal保存/恢复wrapper中以相同-R和-j 1实际执行；-N本身不执行。gpu_viewer_paths等混合矩阵也属于扩展。只有明确要求全量回归，才构建完整目标并在扩展清单上运行全部40项。

专项完成后恢复默认，防止cache残留扩大下一次范围：

```powershell
cmake --preset vs2022-vcpkg -DKUENGINE_ENABLE_GPU_SMOKE_TESTS=ON
ctest --preset debug -N
```

普通preset会把EXTENDED重置OFF；非preset配置需显式 -DKUENGINE_ENABLE_EXTENDED_GPU_TESTS=OFF。恢复后核对16项再运行。关闭GPU总开关且EXTENDED=OFF则仅保留3CPU。

### 判据与未运行项

范围收窄不改变像素、同步、生命周期与失败处理判据。检查实际Validation startup、actual exit、错误数、完成提交/统计、真实画面或独立像素，以及resize/recompile/尾帧。marker、窗口存活、编译通过都不能代替画面正确；既有runner的PASS正则/stderr/assert判据弱点本包未修，必要时另核真实应用退出码。

任务涉及人工交互时，核对两例相关侧栏、比例、输入释放/UI capture、失焦、minimize/restore；FreeFly仅在相关Mclaren任务检查。没有执行的GUI或专项明确标未运行，不能因为历史门禁自动扩大到其他例。

历史结果区分 ACCEPTED、USER-SELFTESTED、重复 SKIPPED、NOT RUN；Release默认无Validation，77skip不是GPU pass。旧报告见[历史记录](../structure/archive/README.md#verification)；本次默认范围工作的独立结果及未运行项集中在[路线图](../structure/roadmap.md#status)。
