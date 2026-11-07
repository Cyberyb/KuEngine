# 产品需求

整理：2026-10-28；REQ-14范围更新：2026-10-30。这里维护“做什么、做到什么程度”；阶段进度只在[路线图](roadmap.md)，已实现契约在[当前架构](../design/README.md)。稳定编号 REQ-01～16 保留，整理文档不增加功能承诺。

<a id="scope"></a>
## 定位与范围

KuEngine 是面向开发者的小型 Vulkan 图形实验框架，支持导入场景、调节材质/相机/灯光、组合渲染 Pass、观察画面与性能。借鉴 RDG 的声明/编译/执行思想，并保留 Vulkan 原生操作。

用户明确要求模型与 HDR 输入、PBR/Unlit、Forward/Deferred、实时性能预览、可收起侧栏、鼠标拖动与 WASDQE 自由漫游。公共 Runtime 应减少设备、同步、生命周期和提交的重复代码。

首轮默认范围：

| 项目 | 边界 |
|---|---|
| 模型与场景 | glTF/GLB、静态多实例/TRS/材质引用；先不引入 ECS |
| 环境 | lat-long .hdr、天空背景与基础环境照明；背景可见和照明独立 |
| 材质 | Metallic-Roughness PBR 与 Unlit；五类纹理/独立UV；Opaque/Mask/Blend |
| 灯光与输出 | 方向光、基础点光、线性 HDR 中间图、曝光/tone map/显示编码 |
| 查看器 | 固定右侧可折叠面板、统一场景视口、Orbit/FreeFly，非任意 Docking 编辑器 |
| 导入入口 | 配置和侧栏路径加载，允许同步加载；失败保留已发布状态 |

HDR 导入不等于 HDR 显示器输出，基础环境照明不等于完整 IBL。阴影/SSAO/Bloom/TAA/光追等仍是[候选](roadmap.md#candidates)。游戏玩法、物理、网络、脚本、完整编辑器与多图形后端不在产品范围。

<a id="requirements"></a>
## 稳定需求清单

P0 是首轮闭环必需，P1 是独立示例或后续强化；不是所有 P0 必须先于 P1。以下“已验收”仅对应各自验收范围；2026-10-30仅验证默认回归选择，未重跑全部历史需求；报告和人工证据边界统一见[验收记录](archive/README.md#verification)。

| 编号 | 功能与目标 | 优先级 | 范围状态 |
|---|---|---|---|
| REQ-01 | 公共 Runtime 与可复用 Vulkan 封装 | P0 | 首轮已验收；示例复用帧循环与 Graph，扩展仍保留原生入口 |
| REQ-02 | 模型导入 | P0 | 首轮已验收；strict scene、多实例共享 mesh、独立 TRS/slot，模型成功替换或失败保留 |
| REQ-03 | 天空盒/HDR 导入 | P0 | 首轮已验收；sky/env独立、线性HDR、真正Clear/Load与失败保留 |
| REQ-04 | PBR、Unlit 多 ShadingModel | P0 | 首轮已验收；实例slot标量与共享材质语义，两路径一致 |
| REQ-05 | 公共 Forward 路径 | P0 | 首轮已验收；HDR、对象级Blend、统一Display、安全切换/编辑 |
| REQ-06 | Deferred 路径 | P0 | 首轮已验收；四MRT/Lighting、共享天空/透明/输出、八debug视图 |
| REQ-07 | 实时性能预览 | P0 | 首轮已验收；FPS/Frame与completed-submit指标分清，无数据明确 |
| REQ-08 | 可折叠侧栏与参数检查 | P0 | 首轮已验收；实例/slot/TRS/材质/相机/灯光、resize/minimize恢复 |
| REQ-09 | 鼠标长按拖动查看 | P0 | 用户修订人工程序下已验收；独立观察、自测/重复豁免分列 |
| REQ-10 | WASDQE 自由摄像机 | P0 | 用户修订人工程序下已验收；Mclaren示例层，不冒充所有示例通用相机 |
| REQ-11 | RDG 风格资源声明与执行 | P0 | 首轮已验收；typed资源/use/range、import/export、同步2/Compute |
| REQ-12 | Vulkan 原生扩展入口 | P0 | 首轮已验收；checked capability/side-effect/use子集，不自动识别任意裸命令 |
| REQ-13 | 小型引擎上的特性开发闭环 | P0 | 首轮已验收；查看器和现有Graphics/Compute/native接入，不承诺任意算法已实现 |
| REQ-14 | 正确性、可复现与扩展观测 | P1 | 部分实现；已有CPU/GPU/像素及双帧子范围证据，逐Pass观测等未完成 |
| REQ-15 | 自建 CornellBox 延迟 PBR 示例 | P1 | 独立示例已验收；程序化房间/两盒/顶灯，复用公共渲染 |
| REQ-16 | Shadertoy 体积光积分复刻示例 | P1 | 独立示例已验收；SebH XlBSRz、参数/固定时间/有界读回、来源与差异披露 |

REQ-14 中涉及 P0 正确性的检查必须随对应功能完成，不得因优先级延后必要同步和生命周期验证。双帧子范围限定1/2帧，不支持3帧；其余进展见[当前阶段](roadmap.md#status)。

2026-10-30 用户调整 REQ-14 的日常示例回归范围：新增功能/特性默认只运行 MclarenApp 和 CornellBoxApp，相关 CPU 契约测试保留；其他示例或混合矩阵需当前特别说明才执行。其他示例及专项测试保留显式入口，未运行不等于通过，也不撤销其功能需求。长期执行约定见 [AGENTS](../../AGENTS.md#testing)，具体命令见[验证方法](../usage/README.md#verification)。

该默认选择子范围已获独立QA ACCEPTED；默认两例实际运行、扩展仅核注册，不代表REQ-14整体完成或其他示例已重验。进度与证据统一见[路线图](roadmap.md#status)。

<a id="acceptance"></a>
## 验收口径

### 资产、材质与灯光（REQ-02～04、08）

- 配置/侧栏可加载受支持模型与 HDR，无需改 C++；失败给出错误并保留可用场景。允许同步加载与安全等待。
- 至少两个实例可独立变换和编辑材质，检查多 primitive/slot；共享 mesh 不强制共享可编辑标量。
- PBR/Unlit 是材质属性。Unlit 不受直接光/环境照明，但仍参与深度、透明和统一输出。
- 可调 base、metallic、roughness、emissive、相机 FOV/速度、灯光颜色/强度、环境/曝光；不适用项禁用或隐藏。
- 天空可见和环境贡献独立；真正移除环境后不引用旧 GPU 资源。

### 渲染路径（REQ-05、06）

- 同一场景切换 Forward/Deferred 保留相机、灯光和材质；受支持输入画面近似一致，不要求逐像素相等。
- 两路径共用坐标、深度、颜色空间/输出与 PBR/Unlit 语义。Unlit 在 Deferred 有明确编码并跳过光照。
- Opaque/Mask/Blend 与 ShadingModel 独立；Mask 裁剪，Blend 首轮走前向透明且声明排序限制。
- GBuffer 记录格式、编码与带宽取舍；深度/HDR/显示/UI 顺序正确，resize/recompile 后有效。

### 查看器与统计（REQ-07～10）

- 侧栏、resize、最小化恢复后视口比例/输入区域一致，控件可用。
- Orbit/FreeFly 切换与复位定义清楚；视口焦点、文本/UI capture、失焦和输入释放不产生残留运动。
- WASDQE/右键转向按帧时间更新，不同帧率速度近似一致；保留原拖动体验。
- 保留已有性能指标与紧凑显示，缺数据 N/A；indexed vertices 是提交索引量，ImGui 不计，CPU/GPU 不混称利用率。

### 算法接入（REQ-01、11～13）与强化（REQ-14）

- 新离屏 Graphics 与 Compute 特性通过声明资源/依赖、Shader/参数和算法命令接入，不复制窗口/交换链/循环/普通屏障。
- 导入明确初始/最终状态，导出明确借用/失效；原生访问声明读写与副作用，能力缺失明确失败/跳过。
- 新 ShadingModel 主要修改材质/Shader/分派，不扩张中央 Runtime 函数。
- 双帧验证真实双槽轮转、完成提交归属、动态资源、两路径/debug、resize/替换与 Validation；不以 FPS 提升验收。
- 批量上传、通用 retire、逐Pass profiler、预设/通用结果导出是后续强化，不能把已有有限读回称作完整实验平台。

### CornellBox（REQ-15）与体积积分（REQ-16）

- CornellBox 自建前开口房间、红左/绿右墙、两个不同旋转/高度盒体和顶灯；绕序/法线/遮挡正确，默认公共 Deferred/PBR，支持参数/debug/统计。
- 顶灯 emissive 与点光分别表达，不宣称面积光积分、阴影、GI 或路径追踪。
- VolumetricIntegration 保留 SebH XlBSRz 场景/介质/动画/改进积分，来源与 CC BY-NC-SA 3.0 许可见 [NOTICE](../../examples/volumetric_integration/NOTICE.md)；噪声替代/固定机位明确。
- 复用 Runtime/Graph/RHI/UI，正确适配 viewport、Vulkan Y、显示编码、1/2帧与resize/recompile；固定时间与参数可比较。
- 两例都需真实 GPU、Validation、非空/独立像素或画面、受影响回归；构建不能替代这些检查。体积积分不承诺原站实时逐像素或通用 Shadertoy 输入。

<a id="changes"></a>
## 需求修改规则

已有编号不重排/复用；新功能使用下一编号，写用户收益、范围、优先级、依赖和可重复验收。撤销/暂缓保留编号。先改需求，再改路线图；实现后同步设计，只有对应 QA 证据充分才能标已验收。

历史建立与修订记录在[整理前归档](archive/README.md#backup)中保留；本次只合并入口和去重，未撤销功能或降低验收口径。
