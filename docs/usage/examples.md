# 八个示例手册

默认功能回归只运行Mclaren与CornellBox；其他示例保留可用，但启动验证需明确要求。范围与扩展命令见[验证方法](README.md#verification)。

公共构建、运行目录、帧槽与 smoke 参数见[开发与运行](README.md)。本页只列示例差异；构建时用下表 target，启动时运行同名 .exe。

| 示例 / target | 适合观察 | 外部素材 | 普通统计（draws/提交顶点或索引） |
|---|---|---|---|
| TriangleApp | 最小绘制、push constants、颜色 UI | 无 | 1/3 |
| CubeApp | 顶点/索引、深度、拖动与线框 | 无 | 随模式 |
| Alpha3PassApp | 三阶段 alpha 与 Graph 依赖 | 无 | 以实际 Pass 为准 |
| MclarenApp | 模型/HDR、PBR/Unlit、查看器与实例编辑 | glTF/GLB、HDR | 默认 Forward91/9202884；Deferred92/9202887 |
| ForwardReuseApp | 同场景 Forward/Deferred 材质语义 | 内置 fixture | Forward9/45；Deferred10/48 |
| GraphResourceProbeApp | Graphics/Transfer/Compute/native 资源链 | 无 | 1/3 |
| CornellBoxApp | 程序化建模、公共 Deferred/PBR | 无 | Forward9/111；Deferred10/114 |
| VolumetricIntegrationApp | 全屏体积光积分比较 | 无；替代噪声 | 2/6 |

统计不含 ImGui；Mclaren 值包含默认 skybox 与 Display，关闭天空少1/3，换模型按实际 SubMesh 核对。测试 readback probe 可能增加 draw，不能套用普通值。

<a id="basic"></a>
## Triangle、Cube、Alpha3Pass

```powershell
cmake --build --preset debug --target TriangleApp CubeApp Alpha3PassApp
Push-Location build/cmake-tools/vs2022-vcpkg/bin/Debug
try { ./TriangleApp.exe --smoke-frames 4 } finally { Pop-Location }
```

Triangle 提供颜色调节。Cube 提供实体/线框及原有鼠标拖动，不提供 Mclaren FreeFly。Alpha3Pass 提供三阶段颜色/透明参数和 Graph 顺序/屏障观察。各例也有 examples/对应目录/run_*.bat，使用旧 build/bin/配置输出。

<a id="viewer"></a>
## 公共场景查看器

Mclaren、ForwardReuse、CornellBox 的 Rendering 控件可切 Forward/Deferred；请求下一安全 update 发布，保留资产、相机、材质、灯光、侧栏，不重载或重编译整图。

Deferred 可选 Scene 和八个 GBuffer 视图：Base、Metallic、World Normal、Roughness、World Position、ShadingModel、Emissive、AO。debug 直接读 MRT、无曝光/tone，Lighting/Blend 仍执行；Forward 禁用 debug 并恢复 Scene。固定映射见[渲染契约](../design/README.md#rendering)。

<a id="mclaren"></a>
## Mclaren：模型、环境与编辑

默认 scene 为 resources/scenes/sandbox/mclaren-sandbox.scene.json，模型 models/props/mclaren_765lt.glb，材质 materials/pbr/mclaren-765lt.material.json，环境 environments/hdr/citrus_orchard_road_puresky_4k.hdr。构建复制已有资源到 exe 旁；修改源码 JSON 后更新运行副本。

从 exe 目录启动，路径按本机仓库位置替换：

```powershell
./MclarenApp.exe --render-path deferred
./MclarenApp.exe --scene D:/GraphcisBibile/Program/UGit/KuEngine/tests/fixtures/viewer-two-instances.scene.json --render-path deferred
```

--scene 缺值/重复退出2；显式 scene 不存在/非法或引用失败退出1，不退回默认车/HDR。相对引用优先 scene 祖先 resources，否则 scene 父目录。显式无环境使用完整黑 fallback；仅旧默认 scene 缺失时保留兼容回退。

Model/HDR 面板的 Draft 与 Active 不同。Load 排队已有普通文件，模型只接受 .gltf/.glb，环境只接受 .hdr；相对路径按 resources root，绝对路径可用。成功才更新 Active/generation，失败保留当前画面和相机。成功 Model Load 创建单节点 replacement-model、instance/slot 选择重置0；HDR/path/resize/recompile 保留实例编辑。

| 专用参数 | 用途 |
|---|---|
| --mclaren-replace-model PATH | 排队模型替换 |
| --mclaren-replace-hdr PATH | 排队环境替换 |
| --mclaren-clear-hdr | 排队真正清除环境 |
| --mclaren-replace-after-updates N | N 次正常 update 后处理下一启动请求 |

```powershell
./MclarenApp.exe --mclaren-replace-model models/props/mclaren_765lt.glb --mclaren-replace-hdr environments/hdr/citrus_orchard_road_puresky_4k.hdr --mclaren-replace-after-updates 1 --smoke-frames 8
```

Clear Environment 通过黑 GPU 候选清 Active HDR、关闭 sky/env 并释放旧资源，不删除用户文件；仅关闭 Skybox/Environment 复选框不等于卸载。Load HDR 可恢复。共享 GPU 资源变化在明确全帧安全点发布，正常帧只等待复用槽。

Material 面板先选 Instance 再选 Material Slot。TRS 改所选实例全部 primitive；slot base/metallic/roughness/emissive/ShadingModel 只改该实例该 slot，同 slot primitive 一起变化，其他实例不变。base/metallic/roughness 在[0,1]，emissive[0,64]，TRS 有限且 scale 正；Unlit 禁用但保留 M/R 值。pending 时相关操作禁用，候选失败不发布。

相机只在场景区域起始操作：

- Orbit Inspect：左拖旋转模型、滚轮缩放；Reset View 恢复配置视图，Reset Model Rotation 只清模型旋转。
- Free Fly：场景右键取得焦点；W/S/A/D 前后左右，Q/E 下上，右拖转向，Move Speed 调速度。
- 模式切换继承当前相机帧，先释放相关输入再开始新操作；点侧栏/文本编辑、UI capture、失焦或恢复取消焦点。

模型面板显示 GPU 容量，公共 Vertices 显示命令提交量。当前不支持任意外部贴图加载、动画、完整 IBL/OIT、HDR 显示器输出或异步导入。

<a id="forward"></a>
## ForwardReuse：材质与双路径 fixture

内置 semantic card：一个共享 mesh、四 primitive、两个实例、两个 GPU 材质 variant。前视检查独立 MR/AO、不对称 emissive UV、Mask hole、Blend；背视仅 cyan double-sided triangles 可见。PBR 随灯变暗，Unlit 保持颜色/emissive。

运行副本 resources/forward_reuse/forward-reuse.scene.json 的 renderPath 可选 forward/deferred，省略 Forward；CLI 优先。View Preset 可选 Semantic front/Double-sided back，UI 可编辑实例 TRS、材质及灯强。

| 专用参数 | 用途 |
|---|---|
| --forward-reuse-zero-lights | 两种直接光都置零 |
| --forward-reuse-directional-only / --forward-reuse-point-only | 分离直接光 |
| --forward-reuse-backface | 背面预设 |
| --scene-sky / --scene-environment | 独立控制天空/环境输入，默认 semantic 无天空 |
| --hdr-fixture | HDR/Blend/opaque 遮挡预设 |
| --normal-fixture | 非平坦法线、UV1、texture transform 预设 |
| --normal-scale N / --display-exposure N | 比较 normalScale 和显示曝光，例如0/1、1/2 |
| --deferred-readback-prefix PATH | 历史名，两路径均可读回；prefix 须含父目录 |

```powershell
./ForwardReuseApp.exe --deferred --hdr-fixture --display-exposure 2 --smoke-frames 3 --smoke-sidebar-collapsed --deferred-readback-prefix C:/temp/kuengine/hdr
```

读回输出 HDR RGBA16F、实际 UNORM/SRGB 显示 PPM、visibility 与 Blend 前后 depth，要求 D32_SFLOAT、最多2048²附件像素；两个额外 Display probes 增加2draws/6vertices。它是有限测试入口，不是通用截图平台。resize/recompile 应更新 relative allocation generation 并重绑，侧栏收展不应额外重绑。

<a id="probe"></a>
## GraphResourceProbe：资源与 Compute/native 接入

固定链为 ImageClear → BufferFill → native Copy128B → Compute Copy32uint → sampled indexed-indirect draw → PostDrawCopy。预期深色背景蓝三角，1draw/3提交索引；不是复杂生成几何，无 host whole-buffer 数值读回。

```powershell
./GraphResourceProbeApp.exe --smoke-frames 8 --smoke-recompile-after-setup --smoke-resize-after 2 800 600
```

READY/COMPUTE_READY local_size32/groups1/RESIZE_OK 是辅助 marker；relative image generation 增加，absolute Buffer 保持。仍须检查实际画面、resize、退出、Validation。启用与恢复 SyncVal 的完整 wrapper 见[验证方法](README.md#verification)。Compute prepare 为 reset-then-build，不承诺 live-recompile 失败恢复。

<a id="cornell"></a>
## CornellBox：程序化 Deferred/PBR

默认1280×960、Deferred，自建前开口房间、红左/绿右墙、白地顶背墙、两个旋转白盒和可见顶灯，不需 JSON/外部模型/HDR。

Scene UI 可调 Ceiling light(0..3)、Exposure(.1..3)、Neutral fill(0..0.5)、两盒独立 Roughness(.04..1)/Metallic(0..1)、camera yaw(-16..16)/pitch(-12..12)/distance(3.2..5.5)；Reset view 恢复0/0/4.1。相机由 UI 控制，无新 Orbit/FreeFly。

四暖点光负责直接光，1×1中性环境补光，顶灯 emissive=(12,10.8,9)；没有阴影、GI、真实面积光积分或完整 IBL。普通 Deferred10/114、Forward9/111，UI 不计。

```powershell
./CornellBoxApp.exe --smoke-frames 4 --cornell-readback-prefix C:/temp/kuengine/cornell
./CornellBoxApp.exe --render-path forward --smoke-frames 3
```

--cornell-readback-prefix 只用于普通 Deferred 有限帧；Forward+读回参数错误。输出 prefix_WIDTHxHEIGHT.rawbin/ppm：rawbin 两个 uint32 width/height，随后 BaseMetallic、PositionModel、线性 HDR 三张全附件 RGBA16F half 平面，每图每像素8B；PPM 裁剪场景、按曝光/Reinhard/sRGB 预览，不含 UI/不是交换链截图。

staging 最多2048²附件像素，按槽 Fence 后读取。--cornell-verify-frame-isolation 是专项测试开关，要求读回 prefix，按提交交替输入用于验证槽隔离，不是普通查看模式。

<a id="volumetric"></a>
## VolumetricIntegration：SebH XlBSRz 体积积分

改编 [SebH / Sébastien Hillaire 的 VolumetricIntegration](https://www.shadertoy.com/view/XlBSRz)。原记录许可 **CC BY-NC-SA 3.0**（署名、非商业、相同方式共享）；精确来源、归档 commit、原 GLSL hash 与修改见 [NOTICE](../../examples/volumetric_integration/NOTICE.md)。这部分不适用项目其余代码的 MIT 许可。

原站和原 iChannel0 纹理访问403，未做原站实时逐像素对照。使用自包含256² repeat/bilinear hash grid 替代噪声，固定相机(20,18,-50)，无原 iMouse 位移。红侧墙/绿地/蓝背墙、移动暖点光、constant/height/sphere 介质与按段解析散射积分保留；没有 Cornell 两盒、独立 opaque 阴影图或通用 Shadertoy 播放器。

默认1280×720、2帧槽，视线100步、光方向16步；侧栏预留场景，读回裁剪尺寸不是窗口尺寸。UI 支持 Pause/Time/Integration、密度/灯强/strong fog、步数、noise/shadows 与 Reset。Split 左 basic 右 improved；Reset 恢复动画time0、improved、density/light1、strongFog0、100/16步、noise/shadows开。

| 专用参数 | 默认 / 范围 |
|---|---|
| --volumetric-time T | 固定时间0..1000000秒；不指定为动画 |
| --volumetric-time-step T | 每实际提交0..100秒，非零要求固定time |
| --volumetric-integration MODE | improved/basic/transmittance-first/split |
| --volumetric-density N | 1 / 0..20 |
| --volumetric-light N | 1 / 0..10 |
| --volumetric-strong-fog N | 0 / 0..10 |
| --volumetric-steps N | 100 / 整数16..256 |
| --volumetric-shadow-steps N | 16 / 整数1..64 |
| --volumetric-no-noise / --volumetric-no-shadows | 关闭噪声/介质阴影积分 |
| --volumetric-readback-prefix PATH | 可选导出，必须正数 smoke-frames |

CLI 与 UI 范围可不同；非有限/越界/缺值参数退出2。time-step 用 time+step×(submittedFrame-1)，非零时即使 Pause 也随实际提交递增。公共 --frames-in-flight 1/2、resize/recompile/Validation 参数可组合。

```powershell
./VolumetricIntegrationApp.exe --smoke-frames 4 --frames-in-flight 2 --volumetric-time 1.25 --volumetric-readback-prefix C:/temp/kuengine/volume
```

每次提交输出 _submitNNNNNN_slotS_WIDTHxHEIGHT 后缀：.rgba16f 为无头 little-endian 交错 RGBA half 线性辐射值，.ppm 为原 pow(1/2.2) gamma 预览，.txt 记提交/槽/尺寸/time/参数/hash/radianceSum。局部64位hash不是 checker 对 raw 字节另算的 SHA256。

数据来自线性 SceneColor，无 UI，不是交换链截图。显示曝光1、无 tone map；UNORM 写 encoded，sRGB 先 inverse-sRGB 再硬件编码，保留原作单次 gamma。Display/Readback 分别读取 SceneColor，不修改公共 PBR 输出契约。

等各槽 Fence 后 inspect，resize/recompile/退出 drain 包括尾帧；析构期失败也使应用失败。2048²上限针对整个附件的 staging 容量，不限普通渲染。无限帧或 smoke-frames0 的读回启动前拒绝。

bat 使用 preset 目录，可用 KUENGINE_BUILD_DIR 指定其他绝对构建目录；Debug/Release 前台启动并返回应用退出码。三个 Shader 使用增量 OUTPUT/DEPENDS。历史独立数值/像素/GUI/bat与2026-10-24格式补验见[验收入口](../structure/archive/README.md#verification)，不把历史结果当作本次重新运行。
