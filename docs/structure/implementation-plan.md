# KuEngine 实施工作包

更新日期：2026-09-14。本页将 [开发路线图](roadmap.md) 拆为可独立实现、验收和文档同步的工作包。需求范围以 [产品需求](product-requirements.md) 为准；工作包状态只在 QA 给出结论后变更。`ACCEPTED` 表示该工作包的验收条件通过，不表示所属阶段已经完成。

## 状态与使用方式

`READY` 为尚未开始，`IMPLEMENTING` 为开发中，`CODE_COMPLETE` 为等待 QA，`ACCEPTED` 为 QA 已验收，`REJECTED` 为需修复，`VERIFICATION_BLOCKED` 为必要验证受环境阻塞。工作包进入 `ACCEPTED` 后才由 Planner 同步当前设计；阶段出口满足前不得将 M0～M5 标为完成。

## M0：基线与最小回归

阶段状态：已完成。QA 已确认 WP01～03 与 M0 出口：CPU/CTest、四示例 Debug GPU Smoke、有限 resize、Validation 失败负例和实际 RTX 4060 Ti 画面/拖动/最小化恢复检查均有证据；无像素参考比对与不支持 Timestamp 的实体设备验证。

| 工作包 | 状态 | 依赖 | 范围与验收焦点 |
|---|---|---|---|
| M0-WP01：Validation 可用性、Debug Messenger 与日志/计数基础 | ACCEPTED | 无 | Debug 配置按实际 Layer/`VK_EXT_debug_utils` 能力选择 Validation；Messenger 生命周期正确，回调日志不跨 C ABI 抛异常，warning/error 原子计数与 Debug 日志级别有单元测试。QA 已验收；未把 Validation 错误自动转为测试失败。 |
| M0-WP02：有限帧运行与 Validation 自动失败 | ACCEPTED | M0-WP01 | `ApplicationRunner` 为四示例提供有限 submitted-frame、Triangle 自动 resize、统一 PASS/FAIL/SKIP/退出码与共享 Validation Tracker；可选 CTest GPU Smoke 覆盖四个应用、受控 Validation error 和缺 Shader 初始化负例。QA 已验收；人工画面与最小化/恢复未由此工作包验收。 |
| M0-WP03：统计样本口径与基线检查 | ACCEPTED | M0-WP02 | 使用 steady_clock；completed-submit 快照将 CPU/GPU/Draw/Vertices 对齐，区分 GPU Unsupported/Waiting/Available，计数饱和；Runner 校验四例预期业务计数并输出 marker。QA 已验收；无每 Pass 曲线、像素回归或实体不支持 Timestamp 覆盖。 |

## M1：查看器交互框架

阶段状态：已完成。QA 已确认 WP01～03 与 M1 阶段出口；侧栏、主视口/输入边界、OrbitInspect/FreeFly 与既有参数/统计均有相应验收依据。

| 工作包 | 状态 | 依赖 | 范围与验收焦点 |
|---|---|---|---|
| M1-WP01：侧栏容器与现有控件迁入 | ACCEPTED | M0-WP02 | 唯一右侧栏承载 Performance、Parameters / Scene、Render Graph；支持展开、紧凑统计、隐藏紧凑统计和重开。Pass 提供 content-only UI，Pipeline 用 Pass 索引隔离 ID。QA 已验收四例唯一侧栏、收展/重开、小窗滚动、Alpha 控件隔离与 Mclaren 分组。 |
| M1-WP02：主视口矩形与输入命中 | ACCEPTED | M1-WP01 | `ViewerLayout` 将逻辑输入 rect 与 framebuffer viewport 对齐；展开保留侧栏、紧凑覆盖，业务 viewport/scissor 限场景而 Overlay 覆盖完整附件。Cube/Mclaren 复用 SceneInteractionGate、滚轮、focus/epoch 边界，Mclaren 删除第二 viewport。QA 已验收比例、无黑缝、Cube UI 隔离/滚轮、resize、最小化恢复；未覆盖 Win+D、物理非等比 DPI、像素比对。 |
| M1-WP03：自由相机与模式切换 | ACCEPTED | M1-WP02 | Mclaren 示例层提供 OrbitInspect/FreeFly、统一 CameraFrame、双向 handoff、右键场景焦点、WASDQE、速度与 reset；UI capture/text、focus/epoch 和 held-neutral 不驱动相机。QA 已验收 CPU Debug/Release、Debug GPU 7/7、Release/缺 Layer 语义与 Mclaren 两向切换/文本/reset/侧栏/窗口检查。持续右拖、六向/组合 held 实机手势未形成可靠证据，仅有永久测试和静态覆盖。 |

## M2：公共资产、材质与 Forward

阶段状态：未开始。出口：受支持模型/HDR 可从配置或侧栏替换，实例可独立调变换/材质，PBR 与 Unlit 复用公共 Forward 装配。

| 工作包 | 状态 | 依赖 | 范围与验收焦点 |
|---|---|---|---|
| M2-WP01：轻量场景与公共 GPU 资源边界 | READY | M1-WP01 | 从 Mclaren 识别并迁出可复用 Mesh、Texture、材质、环境与实例持有职责，不引入 ECS。 |
| M2-WP02：模型/HDR 替换与资源保留规则 | READY | M2-WP01 | 接入配置和侧栏入口；坏路径保留旧场景，明确 GPU 完成后的替换释放边界。 |
| M2-WP03：材质语义与公共 Forward Renderer | READY | M2-WP01 | 独立实例材质/变换、PBR/Unlit、基础光照与环境输入；修复已确认的资产语义缺口。 |

## M3：Graph 管理资源与执行

阶段状态：未开始。出口：Graph 创建并安全管理离屏资源，Graphics/Compute/Copy 的声明依赖可运行并通过 Validation；原生 Vulkan 访问有可检查契约。

| 工作包 | 状态 | 依赖 | 范围与验收焦点 |
|---|---|---|---|
| M3-WP01：图资源描述、Handle 与生命周期 | READY | M2-WP03 | Image/Buffer 描述、外部导入导出、resize 与单帧生命周期。 |
| M3-WP02：状态、同步与 Buffer 依赖 | READY | M3-WP01 | Stage/Access/Layout/子资源用途与 synchronization2；不引入多队列。 |
| M3-WP03：Compute 与原生访问契约 | READY | M3-WP02 | Compute Pipeline、基础绑定、Graphics/Compute/Copy 示例和受约束的原生命令入口。 |
| M3-WP04：Forward 图目标与最终输出 | READY | M3-WP03 | 将 SceneColor/Depth 与 UI/Present 状态交接纳入图。 |

## M4：Deferred 与查看器闭环

阶段状态：未开始。出口：同一场景可切换 Forward/Deferred，PBR/Unlit 语义一致，透明、天空、显示输出和 UI 顺序正确。

| 工作包 | 状态 | 依赖 | 范围与验收焦点 |
|---|---|---|---|
| M4-WP01：最小 GBuffer 与 Deferred Lighting | READY | M3-WP04 | 定义格式与 ShadingModel 编码，完成不透明/Mask 几何及方向光/点光照验证。 |
| M4-WP02：共享材质、透明与显示输出 | READY | M4-WP01 | 对齐两路径的 PBR/Unlit、天空、透明前向阶段、曝光/色调映射与 UI。 |
| M4-WP03：路径切换与 GBuffer 检查 | READY | M4-WP02 | Viewer 选择、GBuffer 调试及完整 REQ-01～13 验收。 |

## M5：多帧安全与实验效率

阶段状态：未开始。出口：2～3 帧资源隔离、路径切换和资源替换无提前复用；可复现实验有足够的性能观测和记录。

| 工作包 | 状态 | 依赖 | 范围与验收焦点 |
|---|---|---|---|
| M5-WP01：按帧资源隔离与多帧提交 | READY | M4-WP03 | UBO、Descriptor、Depth、中间资源、Query 与提交结果按帧槽隔离。 |
| M5-WP02：上传与延迟回收 | READY | M5-WP01 | 批量上传、减少不必要等待、资源替换的安全回收。 |
| M5-WP03：实验观测与 GPU 回归 | READY | M5-WP01 | 逐 Pass 计时、预设/导出、图像比较、能力检查与二次开发说明。 |

## 阶段出口汇总

| 阶段 | 状态 | 满足条件 |
|---|---|---|
| M0 | 已完成 | WP01～03 已验收，四示例 Debug GPU Smoke、有限 resize、Validation/初始化负例及 RTX 4060 Ti 人工检查证据完整；像素回归和不支持 Timestamp 实机验证不属于本出口 |
| M1 | 已完成 | WP01～03 已验收；侧栏、主视口输入边界和 Mclaren 双模式相机出口已满足 |
| M2 | 未开始 | M2-WP01～03 通过 |
| M3 | 未开始 | M3-WP01～04 通过 |
| M4 | 未开始 | M4-WP01～03 通过，并完成 REQ-01～13 整体检查 |
| M5 | 未开始 | M5-WP01～03 通过 |
