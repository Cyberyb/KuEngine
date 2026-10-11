# ForwardReuse 示例

核对2026-10-11。公共资产/Graph、strict配置/CLI/UI安全路径、八GBuffer与共享天空BlendHDR/普通Display沿用既有实现；C新增本例独立可视输出/resize/recompile/正常exit0。REQ-01～13首轮最终QA ACCEPTED + DOCUMENTED，WP03/M4已由Lead最终审计闭环（DONE）。Mclaren编辑/Clear不能由本例控件替代，M5未开始。

从 preset 输出目录启动：

```powershell
cmake --build --preset debug --target ForwardReuseApp
Push-Location build/cmake-tools/vs2022-vcpkg/bin/Debug
try { ./ForwardReuseApp.exe } finally { Pop-Location }
```

自动化有限帧可加入 `--smoke-frames 8 --smoke-require-validation`；追加 `--smoke-recompile-after-setup` 会在 `deviceWaitIdle` 后对同一 Pipeline 第二次 compile，检查 target/descriptor 的释放与重建。`--forward-reuse-zero-lights` 将方向光和点光强度设为零；`--forward-reuse-backface` 选择背面预设。两个参数在通用 smoke 参数之前被应用剥离。

示例 fixture 是 data-URI semantic card：一个 MeshHandle、四个 primitive、两个实例、两个 GPU 材质 variant。Forward 加 Display 后预期每提交帧 **9 draws / 45 vertices**（其中 Display 为一个全屏三角形）。右侧栏的 `View Preset` 可选 `Semantic front` 或 `Double-sided back`；可选 PBR/Unlit 实例并编辑其 position、rotation、scale、base/emissive、shading，以及方向/首个点光强度。

两路径post-UI Prepare生成snapshot，Background CLEAR共享RGBA16F SceneColor → Opaque/Mask或Geometry/Lighting → Forward Blend（Depth STORE→LOAD/test/no-write）→唯一曝光/Reinhard/sRGB Display → Overlay LOAD。曝光不改变显示前HDR，UNORM手动编码，SRGB硬件恒编码/gamma控件禁用。resize引起target generation/descriptor rebind；侧栏收展不额外rebind。recompile保留资产/材质/相机/UI状态，不重新首载；snapshot/独立phase UBO依赖单帧Fence-safe寿命。

人工判断：前视图的 patterned 背景应呈现独立 MR/AO 和不对称 emissive UV；checker hole 应为 Mask；Forward 半透明 tile 应在不透明深度前正确混合。把两种光都置零后 PBR 直射响应变暗而 Unlit 仍可见；背面预设中只有 cyan double-sided triangles 可从背面看到。

## Deferred 验证入口

在 App 输出目录运行（readback prefix 使用含父目录的绝对路径）：

```powershell
$env:VK_LAYER_ENABLES='VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT'
./ForwardReuseApp.exe --deferred --smoke-frames 6 --smoke-require-validation --smoke-recompile-after-setup --smoke-resize-after 2 800 600 --deferred-readback-prefix D:/GraphcisBibile/Program/UGit/KuEngine/build/deferred-evidence/resize
```

Geometry只接收Opaque/Mask，两个Blend由公共Forward合成；无test probes时Deferred10/48、Forward9/45，debug替换一个全屏Display仍10/48。`--scene-sky`与`--scene-environment`独立控制；默认semantic无天空。`--deferred`现为公共CLI兼容别名，不需重新注册图；静态分支经CPU mask选择。

`--forward-reuse-zero-lights`、`--forward-reuse-directional-only`、`--forward-reuse-point-only`比较直接光；`--forward-reuse-backface`检查双面。历史命名`--deferred-readback-prefix`现在两路径均可用，prefix需含父目录，生成HDR RGBA16F、实际UNORM/SRGB显示PPM、visibility和Blend前后depth；marker含`KUENGINE_DEFERRED_PIXELS_OK`。readback要求D32_SFLOAT、最多2048×2048，额外两Display probes为2 draws/6 vertices，不属于普通9/45或10/48计数，也不是通用readback平台。

新增永久预设：`--hdr-fixture`解析HDR/单层双层Blend/opaque遮挡，`--normal-fixture`非平坦法线/UV1/texture-transform；`--normal-scale 0`与默认1比较，`--display-exposure 2`与默认1比较。例如从exe目录执行：

```powershell
./ForwardReuseApp.exe --deferred --hdr-fixture --display-exposure 2 --smoke-frames 3 --smoke-sidebar-collapsed --deferred-readback-prefix D:/GraphcisBibile/Program/UGit/KuEngine/build/hdr-local/deferred-exposure2
```

resize/recompile应改变relative targets generation并输出KUENGINE_DEFERRED_BIND，Lighting/Display重绑；收展不额外rebind。WP02历史两路径/HDR/normal/UV/sky-env/深度/格式/曝光固定阈值通过；最新C本例可视与Mclaren输入范围见[回归](regression-checks.md)。forced live-format/低caps/soak/RenderDoc仍未运行。

## 配置、CLI与真实UI切换（WP03-A）

运行副本`resources/forward_reuse/forward-reuse.scene.json`（源码examples/forward_reuse/assets）的顶层`"renderPath":"forward"`或`"deferred"`实际生效，省略默认Forward；类型/大小写/值严格校验。CLI优先：

```powershell
./ForwardReuseApp.exe --render-path deferred
./ForwardReuseApp.exe --render-path forward
./ForwardReuseApp.exe --deferred
```

`--render-path forward --deferred`冲突失败；非法/缺值也失败（参数退出2），不是静默Forward。相同选择重复允许。配置坏值即使CLI覆盖也不跳过配置解析。路径RenderPath不同于材质PBR/Unlit。侧栏Parameters / Scene→Rendering→Render Path可实际切换；请求显示Pending，到下一Fence-safe update发布，资源identity/材质/TRS/灯光/相机/侧栏保留，不重新上传或compile整个图。无Deferred能力时显示原因/禁用控件，显式Deferred请求失败，默认Forward仍可用；基础HDR能力或初始化allocation失败另论。

Deferred下GBuffer View选Scene或八views：Base RGB、Metallic A、World Normal RGB（n*.5+.5）、Roughness A、World Position RGB（固定world[-2,+2]映射并截断）、ShadingModel A（PBR红/Unlit绿/Empty黑，unknown紫红防御值）、Emissive RGB（[0,1]截断）、AO A。Empty所有views黑；scalar为灰度，所有映射clamp[0,1]。只做单次format-aware sRGB、不做曝光/tone、不采用Blend后的SceneColor；Geometry/Lighting/Blend仍执行，只最终Display换成直接MRT。Forward GBuffer控件禁用解释，debug→Forward恢复Scene、不采样旧MRT。未知model自然GPU fixture不注入，Geometry拒绝不支持值。

A历史官方GUI已操作非默认Double-sided back、F→D→Normal→F/保留preset/sidebar/resize，9图exit0。ViewerPathSmoke同CPU request不冒充点击；8scenarios/200sample actual completedSubmit/rawMRT证据在回归。C最终本例可视输出/正常exit0与Mclaren输入自测/重复豁免分列，不把A记录当C全部操作。
