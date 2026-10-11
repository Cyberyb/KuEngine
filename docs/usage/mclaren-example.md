# Mclaren 示例使用说明

核对2026-10-11。共享Runtime/ScenePasses/HDR/天空Blend、路径/GBuffer、strict scene、实例/slot独立编辑及真正Clear首轮获最终C QA ACCEPTED + DOCUMENTED；REQ-01～13首轮已验收，WP03/M4已由Lead最终审计闭环（DONE），M5未开始。

## 构建和启动

一键运行，在仓库根目录执行：

```powershell
./examples/mclaren/run_mclaren.bat Debug
```

脚本使用 build/bin/Debug，默认前台等待应用退出。可选 `Debug bg` 异步启动；后台脚本退出不表示应用已成功完成渲染。

使用 preset 的完整路径为：

```powershell
cmake --preset vs2022-vcpkg
cmake --build --preset debug --target MclarenApp
Push-Location build/cmake-tools/vs2022-vcpkg/bin/Debug
try { ./MclarenApp.exe } finally { Pop-Location }
```

## 输入资源

路径配置实际读取初始scene顶层`renderPath`，省略默认Forward，存在须精确string`forward`或`deferred`；已有无效scene配置明确启动失败。CLI `--render-path forward|deferred`优先，`--deferred`兼容，冲突/非法/缺值参数错误。示例从exe目录运行`./MclarenApp.exe --render-path deferred`；CLI不跳过严格配置解析。

显式`--scene <scene.json>`支持多SceneInstance。缺值/重复返回2；不存在/非法scene或错误model/material/HDR引用返回初始化失败1，不静默退回默认车/HDR。相对引用根优先scene的祖先resources，无此祖先则scene父目录；显式无environment为完整有效1×1黑环境fallback，不使用默认HDR。无参数旧启动仍使用默认scene，缺失时兼容默认车/HDR；已存在但无效默认scene仍失败。

从exe目录启动两实例、同mesh、每实例3primitive/2slot的永久fixture（仓库路径按实际位置调整）：

```powershell
./MclarenApp.exe --scene D:/GraphcisBibile/Program/UGit/KuEngine/tests/fixtures/viewer-two-instances.scene.json --render-path deferred
```

侧栏Parameters / Scene→Rendering→Render Path请求下一Fence-safe update发布CPU enabled-mask，保留Scene/GPU资产身份、generation/path、材质、灯光、相机姿态/模式/FOV/速度、环境/曝光、sidebar，不重载/上传/compile图。Deferred→GBuffer View支持八通道，具体固定映射与操作见 [ForwardReuse](forward-reuse-example.md#配置cli与真实ui切换wp03-a)；debug直接读MRT，不做曝光/tone/透明颜色但Lighting/Blend仍执行。Forward禁用debug并恢复Scene，无能力提示/拒绝Deferred，初始allocation失败明确失败。

默认场景是 resources/scenes/sandbox/mclaren-sandbox.scene.json，引用模型 models/props/mclaren_765lt.glb 和材质 materials/pbr/mclaren-765lt.material.json。环境为 resources/environments/hdr/citrus_orchard_road_puresky_4k.hdr。

CMake 将已有资源复制到 exe 旁，加载优先使用运行目录副本，再回退源码目录。修改源码 JSON 后，需要更新运行目录副本才会生效。asset-registry.json 会被复制，但示例没有通过 Registry 自动发现资源。

### 模型与 HDR 替换

右侧栏的 Model / HDR 区域分别提供 Draft 路径、Load、Active 路径、generation、状态和错误。填写路径后点击 Load 只排队一个请求；同一时间只能有一个请求。相对路径按当前 resources root 解析，绝对路径可用；模型只接受 `.gltf` / `.glb`，HDR 只接受 `.hdr`，且路径必须指向已有普通文件。

也可在 `MclarenApp.exe` 后传入专用参数；它们会从 Engine 的通用参数中剥离，按给定顺序排队：

```powershell
.\MclarenApp.exe --mclaren-replace-model models/props/mclaren_765lt.glb --mclaren-replace-hdr environments/hdr/citrus_orchard_road_puresky_4k.hdr --mclaren-replace-after-updates 1 --smoke-frames 8 --smoke-require-validation
```

`--mclaren-replace-after-updates N` 在 N 次正常 Pass update 后开始处理下一项启动请求。处理在单帧 Fence 已完成、acquire 后、命令录制前同步进行；它不额外调用常态 `deviceWaitIdle`。成功后更新相应 Active 路径/generation；模型替换会重建模型相关资源和预期统计，HDR 替换只换环境资源。输入、解码、兼容性或上传失败都会保留当前画面、相机、Active 路径、generation 和统计；可选贴图问题可显示 fallback 状态。设备丢失属于致命错误而非保留回退。

HDR区独立`Clear Environment`按钮排队真正移除，不是留空Draft点Load或仅取消Skybox/Environment复选框。下一Fence-safe update构建同layout完整1×1 RGBA32F黑GPU候选，成功才清Active HDR、generation递增、sky/env关闭并释放旧GPU环境；失败保留旧状态。Clear后继续Forward/Deferred/GBuffer、resize/recompile，再填写有效HDR并点Load HDR会恢复环境与sky/env。用户HDR文件不会删除；专用CLI `--mclaren-clear-hdr`按替换队列顺序执行。Active HDR是当前发布环境，初始SceneData.environment不是热替换状态。

### 独立实例与材质slot编辑

成功侧栏Model Load会以单节点`replacement-model`替换SceneData并reset所选instance/slot为0，原双实例编辑不会保留；先完成双实例工作流再替换，或另建明确区分的导入会话。坏Model Load保留当前新模型。HDR/path/resize/recompile才是保留原实例的普通操作。

在Parameters / Scene→Material选Instance，再选Material Slot；Position/Rotation(deg)/Scale控制该实例全部primitive，Slot BaseColor/Metallic/Roughness/Emissive/Shading Model只控制所选实例所选slot。面板显示Shared MeshHandle及Affected primitive，同slot的多个primitive一起变，另一个实例和其他slot不变。base RGBA/metallic/roughness范围[0,1]，emissive RGB[0,64]，TRS必须有限且scale正；Unlit禁用M/R但保留值，返回PBR恢复。Global tint仍全局，不能当作独立slot操作。

操作只排队CPU请求，pending时编辑/资产Load/Clear控件禁用。下一安全update校验候选并准备有限管线和四phase容量，全成功才增长Edit generation；非法instance/slot/数值或准备失败保留旧发布数据。标量/TRS复用共享GPU mesh/texture/descriptor，不重新上传。path/resize/recompile/HDR操作保留编辑与选择；成功model替换重建场景并reset选择，失败不reset。导入fit缓存保持相机，不因实例TRS编辑重新frame。

## 交互与面板

- Camera 区域的 `Mode` 选择 `Orbit Inspect` 或 `Free Fly`。两种模式切换时从当前相机帧交接位置/朝向，切换后先松开所有相关按键/鼠标，再开始新的操作，避免把切换前的 held 输入带入新模式。
- Orbit Inspect：左键必须从场景区域开始拖动，旋转模型；场景区域滚轮缩放。`Reset View` 恢复配置视图，`Reset Model Rotation` 只清模型旋转。
- Free Fly：先在场景区域按下右键取得焦点；保持焦点时 `W/S/A/D` 前后左右、`Q/E` 下上，右键拖动转向，`Move Speed` 调整速度。点击侧栏、编辑文字、UI 捕获键盘/鼠标、窗口失焦或恢复都会取消焦点；重新操作前需要先释放输入。`Reset View` 在该模式下恢复自由相机视图，`Reset Model Rotation` 仍只影响 Orbit 模型旋转。
- 侧栏或被 UI 捕获区域不驱动相机。投影比例与交换链场景 viewport 共享同一布局，不再存在第二套 Mclaren 局部视口。
- 控制 BaseColor、Normal、ORM 采样，UV-Y 翻转与输出 Gamma。
- 调整相机投影、可见视口、光照和全局颜色因子。
- 开关 Skybox、环境反射并调整环境强度/曝光。
- 查看模型 Vertex/Index/SubMesh 数量和加载错误。
- 右侧唯一侧栏提供 Performance、Parameters / Scene、Render Graph；可收起为紧凑统计或仅重开控件。公共统计显示 FPS、Frame、CPU/GPU 时间与业务 draw/顶点提交数。

公共Vertices是提交索引/顶点量，模型面板是GPU容量；CPU/GPU/Draw/Vertices同一completed submit，FPS/Frame当前主循环样本，口径见 [UI设计](../design/05-ui-layer.md)。默认资源/天空/无probe时Forward91/9,202,884，Deferred92/9,202,887（多Lighting三角形）；含Display，关闭Skybox少1/3，替换模型后按实际SubMesh动态核对。

HDR SceneColor保留线性>1至唯一Display曝光/Reinhard/sRGB，UI后合成。Skybox可见与环境贡献独立，但开关不等于实际卸载HDR。UNORM可手动gamma，SRGB硬件编码恒启用/gamma开关禁用。`--smoke-recompile-after-setup --smoke-resize-after 2 800 600 --smoke-frames 6`可在两启动路径核对重复compile/resize；首次initialize加载资源，ready重编译保留模型/材质/相机/UI与替换状态，各phase独立UBO，候选在Fence-safe update成功才发布。

## 当前限制

最终C2026-10-11已ACCEPTED：A3同PID63840双实例最终gen19、八rawviews/灯光相机/统计/true minimize/restore，normal0/SMOKE3562615/error0；另一个fresh-default SessionB PID80472完成LoadA→Clear→F/D/raw→真实2560×1369→1280×720→LoadB→badHDR、成功model request5单节点/reset0→badmodel request6保留新car，normal0/SMOKE401459/error0。不声称A3gen19跨进程保持；edited-Clear完整数值流程继承B冻结证据。独立Orbit/释放/UI-origin/W/S，WASDQE/right-look/UItext/focus USER-SELFTESTED、重复SKIPPED；专项held stress/exact rates新实机NOT RUN，详见[回归](regression-checks.md)。Vulkanwarning1为duplicate EOS loader，不涵盖模型缺UV的应用warnings；正常候选失败保留不是物理OOM/device-loss实测。

emissive/normal/TBN/TRS/material override/独立UV/对象排序已接入；A路径/GBuffer与B最终实例/slot/TRS、strict scene及真正Clear已局部验收。不是通用ECS/编辑器，任意外部贴图路径/异步加载/跨场景缓存未实现。完整IBL/OIT/HDR显示器输出不属首轮承诺。

Free Fly仍只属Mclaren示例层，Cube未接入；早期M1/A/B记录保留日期化历史，当前C范围见前述证据与回归。独立持续Orbit/释放/UI-origin/W/S与用户自测WASDQE/right-look/text/focus明确区分，不把CPU或旧手势冒充全部独立实机。新专项diagonal/startup/mode/reset/epoch-held stress/exact physical rates、OOM/device-loss/soak/Win+D/物理非等比DPI NOT RUN。

WP02四个app/path live窗口/侧栏/maximize是历史局部证据；最新C普通true minimize/restore与实际resize另有实机证据，用户自测/重复豁免见前述证据与回归。forced live-format/soak/RenderDoc/低能力实机仍未运行。Depth归Graph pool，Runtime只选择format，不重复创建交换链深度。完整设计见[示例](../design/04-triangle-example-tech.md)与[PBR](../design/10-pbr-rendering.md)。

运行检查见 [回归指南](regression-checks.md)，源码调试见 [Shader 调试](shader-debugging.md)。

历史A局部验收（2026-10-08；以下C未运行仅当时，最新见本页当前限制与回归）：A官方GUI十五图：实际切FreeFly/FOV45.926/exposure1.747，F→D、八views、debug→Forward恢复正常car/sky，参数保留；collapse/reopen/maximize/restore实际resize正确、exit0/errors0。与自动smoke FreeFly/FOV47/speed5.3/exposure1.7不同，不互相替代。完整held/text/focus/minimize未运行（C），低能力/alloc-fault/live-format/soak/RenderDoc未运行。

历史B局部验收（2026-10-10；以下C待项仅当时，最新见本页当前限制与回归）：B最新正式独立QA(2026-10-10)最终同一GUI33原图：两实例各TRS/base/met/rough/emissive/PBR-Unlit恢复、slot联动/另一实例保持、Unlit禁用值保留、path/GBuffer/sidebar/maximize-restore、LoadA→Clear→path/GBuffer/resize→LoadB→badHDR保留；最终原会话实际app exit0、显式SyncVal errors0，warning1为系统EOSOverlay重复loader。永久两layout/config各29samples/16edits，独立像素/资源寿命检查见回归。旧QAB-01 REJECTED、原fullGUI退出unknown、partialGUI后来0、final02 locked blocked均保留历史；不拼接旧会话冒充最终通过。C持续held/textcapture/focus/minimize未运行，物理GPU OOM/device-loss注入未运行，正常候选失败保留不是其证明。
