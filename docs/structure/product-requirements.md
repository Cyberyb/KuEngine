# KuEngine 产品需求

更新日期：2026-10-24。本文是产品目标与功能范围的维护入口；REQ-01～13首个产品范围已获最终C QA ACCEPTED，C文档已同步，M4已由Lead最终审计闭环（DONE），M5未开始。新增 REQ-15 康奈尔盒子示例，独立于 M5 实施。当前实现见 [宏观架构](current-architecture.md)，开发顺序见 [roadmap](roadmap.md)，架构取舍见 [目标架构](target-architecture.md)。

## 1. 产品定位

KuEngine 是一个基于 Vulkan 的小型渲染引擎，面向希望快速开发、组合和验证图形渲染特性的开发者。

引擎提供可复用的资产、材质、渲染路径、调试界面和公共 Runtime，减少设备初始化、资源生命周期、同步和提交等重复工作；开发者仍然能够访问 Vulkan 的原生能力。架构借鉴 UE RDG 的资源声明与图执行思想，但不追求复刻 UE，也不发展为游戏引擎。

核心使用流程：导入模型与环境 → 选择渲染路径和材质 → 调整相机与灯光 → 插入自定义渲染特性 → 观察画面与性能。

## 2. 范围与约定

### 2.1 用户明确要求

- 模型导入、天空盒/HDR 导入。
- 多 ShadingModel，首先提供传统 PBR 与 Unlit（无光照）。
- 前向渲染与延迟渲染。
- 保留已集成的实时性能预览。
- 可收起、展开的侧边 UI，调整材质、摄像机、灯光等参数。
- 保留现有鼠标长按拖动查看方式，新增 WASDQE 自由摄像机漫游。
- RDG 风格的开发接口，同时保留 Vulkan 特有功能和原生访问自由度。
- 结构轻量，服务渲染实验，不承担完整游戏引擎的职责。

### 2.2 本轮规划采用的默认值

以下是为了形成可执行路线而选定的设计默认值，后续可以修改，不是用户已经限定的唯一方案。

| 项目 | 首轮范围 |
|---|---|
| 模型格式 | 先把已有 glTF/GLB 链路产品化；不一次覆盖 FBX、OBJ 等所有格式 |
| 环境输入 | 先支持已有等距柱状投影 `.hdr`，可用于天空背景与环境照明；六面图、DDS/KTX、EXR 后续按需扩展 |
| 导入入口 | 同时保留配置文件和侧边栏路径输入/加载；原生文件选择器、拖拽文件不是首轮依赖 |
| PBR | Metallic-Roughness，统一基础色、金属度、粗糙度、法线、AO、emissive 的语义 |
| 灯光 | 先统一现有方向光及环境参数，再增加点光源；不先做复杂灯光编辑器 |
| 场景数据 | 静态模型实例、变换、材质引用、相机、灯光、环境；允许多个实例，不引入 ECS |
| UI | 默认右侧固定面板、可收起；主视口尺寸随之调整，不要求任意拖拽 Docking |
| 漫游输入 | WASD 前后左右，Q 下移、E 上移；按住鼠标右键转向，速度可调；移动量依赖帧时间 |
| 渲染路径选择 | 由配置及 UI 选择 Forward / Deferred，场景与材质数据不随路径切换而重建语义 |
| 现代管线首轮闭环 | 深度、PBR/Unlit、HDR 环境、前向/延迟、基础透明处理、统一曝光/色调映射与显示输出 |

HDR 资源导入不等于 HDR 显示器输出。首轮保留线性高动态范围中间结果，统一转换到显示输出；HDR Swapchain、显示器元数据不在当前承诺内。完整 IBL 预计算也不等于“能够加载 HDR”，需要单独排期。

### 2.3 明确不做与候选范围

不建设游戏玩法框架、物理系统、网络同步、脚本运行时、完整关卡编辑器、资产烘焙平台或多图形后端。侧边栏是渲染检查与调参工具，不是游戏编辑器。

阴影、完整 IBL、SSAO、Bloom、抗锯齿等属于现代渲染的合理后续候选，但“现代管线”不意味着一次承诺全部特效。它们在 [候选池](roadmap.md#5-后续候选池) 中单独管理。光追、Mesh Shader、Bindless 等也不作为首轮完成条件。

## 3. 功能需求清单

编号稳定保留，修改描述不重新编号。状态使用“未实现 / 部分实现 / 已实现 / 已验收 / 暂缓 / 撤销”；“已实现”不代表本轮重新完成运行验收。下表按当前源码与设计文档建立基线。

P0 表示首个完整产品闭环必需，不表示所有 P0 都必须先于所有 P1 开始；P1 为闭环后的强化，具体顺序由 roadmap 决定。

| 编号 | 功能与目标 | 优先级 | 当前状态与缺口 | 阶段 |
|---|---|---|---|---|
| REQ-01 | 公共 Runtime 与可复用 Vulkan 封装 | P0 | 已验收（首轮）：六例复用Runtime/Graph；编辑与验证仍示例层；接入说明已同步，多帧属M5 | M2、M3、M4 |
| REQ-02 | 模型导入 | P0 | 已验收（首轮）：strict scene、两实例共享mesh/独立TRS与多slot；实际侧栏模型成功替换重建单节点/重置选择，失败保留新模型；异步/跨场景缓存非首轮依赖 | M2、M4、M5 |
| REQ-03 | 天空盒/HDR 导入 | P0 | 已验收（首轮）：sky/env独立、线性HDR、真正Clear黑GPU fallback/旧GPU释放/Load恢复及失败保留；完整IBL/HDR显示器输出非首轮承诺 | M2、M4 |
| REQ-04 | 多 ShadingModel：PBR、Unlit | P0 | 已验收（首轮）：实例slot独立标量、Unlit禁用但保留M/R、共享着色与显示，两路径灯光/环境响应 | M2、M4 |
| REQ-05 | 公共前向渲染路径 | P0 | 已验收（首轮）：公共HDR/对象级Blend/唯一Display、安全切换/编辑与资源保留，固定像素阈值 | M2、M3、M4 |
| REQ-06 | 延迟渲染路径 | P0 | 已验收（首轮）：四RGBA16F/Lighting/共享天空BlendHDR/直接MRT八views及resize-recompile | M4 |
| REQ-07 | 实时性能参数预览 | P0 | 已验收（首轮）：FPS/Frame当前loop；CPU/GPU/Draw/Vertices同一completed submit，waiting/unsupported明确，完整/紧凑统计可操作 | M0 完成、M1 集成、M4 |
| REQ-08 | 可折叠侧边 UI 与参数检查 | P0 | 已验收（首轮）：实例/slot/材质/TRS/相机/灯光、收展/隐藏/重开、真实resize与普通true minimize/restore | M1、M2、M4 |
| REQ-09 | 鼠标长按拖动查看 | P0 | 已验收（用户修订人工程序）：独立持续Orbit/滚轮/释放/UI-origin隔离；失焦隔离USER-SELFTESTED、重复SKIPPED；专项held stress未运行 | M1 完成、M4 |
| REQ-10 | WASDQE 自由摄像机 | P0 | 已验收（用户修订人工程序）：独立W/S保持与释放、neutral handoff/reset；WASDQE/right-held look/UI文本capture/失焦USER-SELFTESTED、重复SKIPPED；18controller CPU测试证明数学/状态契约，不冒充全实机。仍Mclaren示例层 | M1 完成、M4 |
| REQ-11 | RDG 风格的资源声明与执行 | P0 | 已验收（首轮）：typed allocation/use/range、import/export/resize、sync2与Compute；资源别名/多Queue不作为首轮条件，尚未实现 | M3、M4 |
| REQ-12 | Vulkan 原生扩展入口 | P0 | 已验收（首轮）：checked native capability/side-effect/use子集与执行期借用；after-execute失效为源码核对，专项永久误用测试缺席；不自动检查任意裸Vulkan | M3、M4 |
| REQ-13 | 小型引擎上的特性开发闭环 | P0 | 已验收（首轮）：最终需求矩阵、完整查看器工作流与现有Graphics/Compute/native接入说明；不是任意新特性已实现声明 | M3、M4 |
| REQ-14 | 正确性、可复现与扩展性能观测 | P1 | 部分实现：CPU/Smoke/SyncVal与WP02有界HDR/格式/深度/多材质像素检查已验收；非通用历史图像平台。逐帧隔离/逐Pass观测/低能力Timestamp实机仍属后续强化 | M0 完成、M5 强化 |
| REQ-15 | 自建康奈尔盒子延迟 PBR 示例 | P1 | 已验收（2026-10-21 独立 QA ACCEPTED）：代码生成房间、红绿侧墙、两个旋转盒体和可见顶灯；默认 Deferred，复用公共 PBR / GBuffer / HDR 输出 | 独立示例工作包 CornellBox |
| REQ-16 | Shadertoy 体积光积分复刻示例 | P1 | 已验收（2026-10-24 独立 QA ACCEPTED）：SebH XlBSRz 场景/体积积分、参数侧栏、固定时间与有界像素导出；替代噪声/固定机位/许可和原站 403 边界已披露；DOCUMENTED，Lead 最终核对待完成 | 独立示例工作包 VolumetricIntegration |

REQ-14 中影响 P0 功能正确性的测试随对应阶段完成，不能以 P1 为由延后必要的同步和生命周期验证。

2026-10-21 用户授权启动 REQ-14 的双帧子范围（M5-WP01）：仅实现最多 2 帧在途，保留单帧模式用于兼容和对照，不支持 3 帧。常规循环只等待复用帧槽，命令、动态 UBO、可变 Descriptor、Depth/HDR/GBuffer、Query 与完成提交统计按槽隔离。资源替换、编辑造成共享 GPU 对象变化、resize/recompile/退出须在明确安全点处理，允许这些低频操作等待全部在途工作。验收覆盖实际双槽轮转、完成提交归属、两渲染路径、调试输出、resize、资产替换与 Validation；不以 FPS 涨幅作为通过条件。批量上传、通用延迟回收、逐 Pass profiler、预设/导出和 M5 其他功能不在本次范围。当前状态：实施中，未验收。

2026-10-11最终`build/qa-m4-wp03c-final/qa-report.md`与`req01-13-qa-matrix.md` ACCEPTED，依据当前冻结C276、A3与独立SessionB正常退出0/SyncVal error0、五新可视示例和明确继承的B回归。用户明确自测WASDQE全正确并跳过重复，又确认右键持续转向/UI文本capture/失焦正确并免重复人工；记录为USER-SELFTESTED/重复SKIPPED，不删除原功能需求、不扩大为GPU/模型验证豁免。专项diagonal/startup/mode/reset/epoch-held物理stress与exact rates新实机NOT RUN；18controller tests位于composite40证明数学/状态契约。Release CTest3pass/28skip77不是GPU通过，另20真实Release GPU组actual0；不是C全部重跑。REQ-14仍部分/M5未来，M4最终Lead闭环完成，M5未开始。物理OOM/device-loss/低caps/live-format/soak/RenderDoc未运行。具体会话/继承/限制见 [回归说明](../usage/regression-checks.md)，接入见 [开发渲染特性](../usage/developing-render-features.md)。

## 4. 验收口径

### 4.1 资产、材质与灯光（REQ-02～04、08）

- 不修改 C++ 即可通过配置或侧边栏加载受支持模型、替换 HDR；失败时显示可理解的错误并保留可用场景。首轮允许同步加载和安全等待，不要求后台流式导入。
- 至少两个模型实例可以独立调整变换与材质；共享资产不意味着必须共享可编辑材质参数。检查多 primitive、多材质模型，不只验证单个 Mclaren。
- PBR 与 Unlit 是材质属性。Unlit 不受直接灯光和环境照明影响，但仍参与深度、透明策略和统一显示输出。不可将“关闭灯光”冒充独立 ShadingModel。
- 侧栏可以选择实例/材质，调整基础色、金属度、粗糙度、emissive、相机 FOV/速度、灯光颜色/强度与环境曝光；不适用的参数隐藏或禁用。
- 分开控制天空背景可见性与环境照明贡献。移除环境时仍可运行，不引用已销毁的 GPU 资源。

### 4.2 渲染路径（REQ-05、06）

ShadingModel 回答“材质怎样响应光照”；RenderPath 回答“这些计算组织在哪些 Pass 中”。两者不能合并为一个枚举，也不要求每种材质创建一个独立 Renderer。

| 组合 | 首轮预期行为 |
|---|---|
| Forward + PBR | 几何绘制阶段完成 PBR 光照 |
| Forward + Unlit | 输出材质颜色/emissive，不计算光照 |
| Deferred + PBR | 几何阶段写 GBuffer，光照阶段读取并计算 PBR |
| Deferred + Unlit | 明确标识无光照表面，跳过光照并输出材质颜色/emissive |

- 同一测试场景可切换路径，保留材质、灯光、相机状态；允许安全同步后重建路径相关资源，不承诺无停顿热切换。
- 两条路径统一坐标、深度、颜色空间和输出处理；相同受支持输入的画面应近似一致，不要求逐像素完全相等。
- Opaque / Mask / Blend 与 ShadingModel 独立。Mask 是裁剪表面，不直接归类为半透明；Deferred 中 Blend 首轮走前向透明阶段，明确排序与不支持复杂透明交叠的限制。
- GBuffer 初版只覆盖这些需求，不提前固定最终通道布局；实现时记录格式、带宽取舍和 ShadingModel 编码。

### 4.3 查看器与性能（REQ-07～10）

- 侧栏收起/展开、窗口 resize、最小化恢复后，画面比例正确、控件仍可操作；渲染区域与输入命中区域一致。
- 拖动查看与自由漫游可切换；定义进入模式、复位和姿态保留规则，避免切换时不可解释的跳变。保留当前拖动体验，不将旋转模型悄悄改成移动相机。
- 自由漫游只在视口获得输入时生效；编辑文本、拖动 UI、窗口失焦时不移动相机、不残留按键状态；不同帧率下移动速度近似一致。
- 保留所有已有性能指标；侧栏收起后仍能通过紧凑统计条查看，允许主动关闭。CPU/GPU 无有效数据时显示 N/A。
- Draw Calls/Vertices 保持当前“业务命令提交量”口径：indexed draw 统计索引提交数而非去重顶点，ImGui 绘制当前不计入；CPU/GPU 不混称为整个程序消耗或纯 Shader 时间。口径变化需同步 [UI 设计](../design/05-ui-layer.md)。

### 4.4 开发者体验与 Vulkan 自由度（REQ-01、11～13）

- 新增一个离屏图形特性和一个 Compute 示例，只需声明资源/读写、设置 Shader/参数、记录算法命令，不复制窗口、Swapchain、帧循环和常规跨 Pass 屏障代码。
- 可导入外部 Image/Buffer，并约定初始/最终状态；图内临时资源由 Graph 管理。自定义原生命令只能访问当前执行范围内合法的资源，必须声明副作用。
- 至少提供一个带能力检查的 Vulkan 原生命令接入示例；设备不支持时明确跳过或报错，不默默改变算法。首轮不要求为每个 Vulkan 扩展编写封装。
- 新增 ShadingModel 的改动集中在材质、Shader 与必要的渲染分派，不要求改动 Runtime 帧循环。新增 Pass 不要求编辑一个巨大的中央绘制函数。

### 4.5 康奈尔盒子示例（REQ-15）

- 在 `examples/cornell_box` 提供独立可构建、可启动的 `CornellBoxApp`，自行生成几何与材质，不依赖下载模型或外部 HDR。
- 场景包含前方开口的房间、红色左墙、绿色右墙、白色地板/天花板/背墙、两个高度和旋转不同的盒体及可见顶灯；法线、绕序与深度遮挡正确。
- 默认使用公共 Deferred Geometry → Lighting → HDR Display；表面使用公共 Metallic-Roughness PBR，保留 GBuffer 检查与实时统计。灯光及材质、曝光可调。
- 顶灯发光表面与公共点光源共同表达照明；当前共享 PBR 近似保持既有语义，不将其宣称为真实面光源积分、阴影、全局光照或路径追踪结果。
- 通过场景几何/材质 CPU 检查、真实 GPU 有限帧运行、Validation、resize/recompile 和可定位的画面检查后才能标记已验收；原有示例的相关回归保持通过。

当前状态：已验收。2026-10-21 [独立 QA ACCEPTED](../../build/qa-cornell-box/qa-report.md)：Debug定向CTest7/7（含Triangle/ForwardReuse相关回归），两配置core142/142，显式SyncVal errors=0，真实Release两路径/resize-recompile、独立GPU几何/像素和GUI控件通过；设计/usage已同步。未执行完整33项CTest，其余范围见[回归](../usage/regression-checks.md)。

### 4.6 Shadertoy 体积光积分示例（REQ-16）

- 在 `examples/volumetric_integration` 提供独立的 `VolumetricIntegrationApp`，以 [SebH 的 XlBSRz](https://www.shadertoy.com/view/XlBSRz) 为场景和体积积分参考；保留作者、来源与所取得源码的许可说明，明确复刻差异和来源证据。
- 复用公共 ApplicationRunner、RenderGraph、RHI 与 UIOverlay，示例自身负责全屏 Shader、程序化场景与介质，不将实验性体积效果扩展为公共 PBR 或通用 Shadertoy 播放器。
- 保留参考的几何、灯光、介质、动画及改进积分核心；侧栏支持常用参数与复位，并有固定时间入口便于画面对比。噪声等外部输入应明确来源，或说明自包含替代产生的差异。
- 正确处理主视口坐标、Vulkan Y 方向、交换链输出编码、resize/recompile 和 1/2 帧在途；不复制窗口、Swapchain 或帧循环。
- 验收包含 Shader 构建、实际 GPU 有限帧与非空画面检查、Validation、resize/recompile、固定时间动画/参数响应，以及受影响的已有示例回归。构建通过不能代替视觉或 GPU 验收。

当前状态：已验收，2026-10-24 [独立 QA ACCEPTED](../../build/qa-volumetric-integration/qa-report.md) + DOCUMENTED，Lead 最终核对待完成。两配置 core145/options4、Debug 定向 CTest8/8/Release3/3（0skip）、各11场景36提交、Debug 显式 SyncVal0error、无噪声原作 CPU 参考890像素、PPM编码全部645120像素字节误差0及实际 GUI/bat退出0。原站/原texture403，公开归档来源与许可见 [NOTICE](../../examples/volumetric_integration/NOTICE.md)；未运行原站逐像素或强制 sRGB attachment。详细范围见 [usage](../usage/volumetric-integration-example.md)。不改变既有 M5 工作包的状态或验收范围。

## 5. 如何修改或增加需求

本文管理“做什么、做到什么程度”，[目标架构](target-architecture.md) 管理“采用什么边界和约束”，[roadmap](roadmap.md) 管理“先后顺序与阶段验收”；不要在三个文件重复维护同一份进度表。

1. 修改已有功能时保留 REQ 编号；新功能使用下一个编号。说明用户收益、首轮范围、验收场景和优先级。
2. 区分必需功能、设计默认值和候选项；明确哪些旧需求被替代。取消的编号保留并标记暂缓/撤销，不复用。
3. 检查对资产格式、材质、两条渲染路径、Graph、UI 和能力要求的影响，再调整路线图阶段和依赖。
4. 需求变更不直接改写 design 为已实现；代码落地后同步对应 design、当前宏观架构、需求状态与阶段状态，并记录当日日志。
5. 只有完成对应验收，才将状态从“已实现”改为“已验收”；附可定位的测试、使用说明或日志依据。

新增需求可使用下面的简短模板，再将摘要加入清单：

```markdown
### REQ-XX：功能名称

- 用户收益：解决什么开发或验证问题。
- 范围：首轮包含什么，明确不包含什么。
- 优先级 / 状态 / 阶段：P0 或 P1 / 未实现 / Mx。
- 验收：可以重复检查的输入、操作和预期结果。
- 影响与依赖：涉及哪些已有需求、模块与设备能力。
```

### 需求修订记录

| 日期 | 变更 |
|---|---|
| 2026-09-13 | 根据产品目标建立 REQ-01～14，明确小型 Vulkan 渲染引擎定位、双渲染路径、查看器及 RDG 风格扩展边界 |
| 2026-10-11 | REQ-01～13首轮范围依最终C QA已验收；仅修订人工重复程序，保留功能需求、独立证据/用户自测/未运行边界；REQ-14仍部分，M5未开始 |
| 2026-10-21 | 新增 REQ-15：自建康奈尔盒子场景，默认复用延迟 PBR，独立示例工作包，不启动 M5 |
| 2026-10-24 | 新增 REQ-16：复刻 Shadertoy XlBSRz 的独立体积光积分示例，验证与许可边界单独记录 |
