# 路线图与未来架构约束

整理：2026-10-28。这里集中维护阶段状态、后续顺序和未实现的架构方向。产品范围见[需求](product-requirements.md)，当前代码组织见[架构](../design/README.md)。M0～M5 是工作阶段，CMake/版本宏仍为0.1.0，不据历史v0.2/v0.3称谓宣布发布。

<a id="status"></a>
## 当前阶段

| 阶段或工作包 | 状态 | 已确认范围 |
|---|---|---|
| M0 基线 | 已完成 | CPU/有限GPU/Validation最小诊断与统计口径 |
| M1 查看器交互 | 已完成 | 侧栏/统一视口、Mclaren Orbit/FreeFly与输入边界 |
| M2 公共资产与Forward | 已完成 | Scene/窄GPU资源、同步替换、材质语义、公共Renderer及第二消费者 |
| M3 图资源与执行 | 已完成 | typed allocation/lifecycle、同步2、Compute/callback/native、Graph-owned输出 |
| M4 Deferred与产品闭环 | 已完成 | REQ-01～13首轮；WP01/02/03-A/B/C门禁，2026-10-11 Lead闭环 |
| M5-WP01 双帧子范围 | 代码已实现；已有独立 QA ACCEPTED | 默认2、兼容1、真实双槽/按槽资源/完成统计；原任务Lead收尾不由本次文档整理追认 |
| M5 整体 | 未完成 | 批量上传、通用回收、逐Pass观测、预设/结果复现等仍未完成 |
| CornellBox / REQ-15 | 独立示例已验收并闭环 | 2026-10-21，程序化场景复用公共Deferred/PBR |
| VolumetricIntegration / REQ-16 | 独立示例已验收并闭环 | 2026-10-24，来源/差异/专用显示/有界读回；含格式补验 |
| 默认示例回归范围 / REQ-14 | 已实现，独立QA ACCEPTED | 2026-10-30 用户要求：默认仅 Mclaren/CornellBox，CPU 契约保留；其他示例与混合矩阵显式开启 |

双帧状态依据当前源码与 [2026-10-21独立QA](../../build/qa-m5-dual-frame/qa-report.md)。旧文档中“M5未开始/未验收”的阶段快照保留在归档，不再当作当前代码能力。该子范围 ACCEPTED 不等于 M5 整体完成，也不是本次重新跑过GPU。

2026-10-30默认回归子范围依据[独立QA报告](../../build/qa-test-default-viewers-2026-10-30/qa-report.md)：GPU关闭3项、默认16项、扩展原40项仅注册、preset恢复16项、非法组合拒绝、默认禁用Python仍配置成功。独立Debug构建与串行显式SyncVal默认16/16通过（0失败/0skip，CPU189用例），仅实际启动Mclaren/CornellBox；5个直接App另核真实exit0。未执行扩展/其他六例、Release或人工GUI，既有runner判据弱点未修；REQ-14仍部分实现，M5整体仍未完成。Lead另在普通preset目录核对16项注册，未在该目录重复GPU运行。

历史局部验收、人工自测/豁免、未运行项及原报告统一见[验收记录](archive/README.md#verification)，不在各篇重复测试流水。

<a id="order"></a>
## 开发顺序与出口

```mermaid
flowchart LR
    M0["M0 基线"] --> M1["M1 交互"] --> M2["M2 资产/Forward"]
    M2 --> M3["M3 图资源/同步"] --> M4["M4 Deferred闭环"] --> M5["M5 实验效率"]
```

这表示主要交付顺序。M2 的CPU资产与M3状态模型可独立演进；M4依赖统一材质与真实中间图。正确性检查跟随每阶段，不能积压到M5。

| 后续工作范围 | 出口要求 |
|---|---|
| 双帧子范围收尾 | 保留原QA证据及Lead/文档状态边界，核对1/2帧、resize、替换、debug与正常退出 |
| 上传与资源回收 | 批量化减少queueWaitIdle；资源在GPU完成后回收，有明确完成边界和失败行为 |
| 性能与复现 | 逐Pass计时/曲线、参数预设、结果导出；记录设备、分辨率、同步/呈现条件 |
| 二次开发效率 | 以第二个真实算法验证公共API；完善图像对比、能力检查与开发说明 |

以上仍需按具体需求拆工作包与验收，不视为本次已批准实施的功能。原 M4/M5 任务包仅保留[历史副本](archive/README.md#tasks)，恢复工作时以当前用户授权与源码重新确认范围。

<a id="architecture"></a>
## 后续实现应保持的架构约束

- 公共 Runtime 管生命周期/提交，算法只装配 Pass；不在 Engine 中加入算法专属中央分支。
- 声明、编译、执行分离。资源/副作用必须可见，原生访问保留明确状态和同步交接。
- 长期资产归 Source/资源对象，临时附件归 Graph；导出是借用，不是所有权或退役句柄。
- Forward/Deferred 共用场景、材质/灯光语义、资产与输出，差异集中在 Pass 与中间图。新增模型说明 GBuffer 表达或 Forward-only 限制。
- 帧槽、交换链image与实际submit编号分离；可变GPU资源和Query按槽，呈现信号按image寿命。正常帧不全局idle，低频共享对象替换可全帧等候。
- 新特性先查询并显式启用设备能力，能力缺失给出失败/跳过/有标注的回退。原生句柄不能代替设备创建期的feature协商。
- 新资源形态与参数封装由实际算法驱动，验证失败回滚、resize、跨帧寿命和第二消费者，不靠增加类数量证明进展。
- CornellBox 保持示例层程序化场景；体积积分保持独立Pass与专用参考显示，不扩大公共PBR或通用Shadertoy承诺。
- 当前轻量定位保持，不同时引入多后端、完整ECS、节点编辑器、通用反射和任务系统。

上述是未来工作的约束；尚未存在的类/接口不能写入当前设计为已实现。

<a id="candidates"></a>
## 后续候选池

| 方向 | 候选 | 纳入前需明确 |
|---|---|---|
| 光照 | 阴影、IBL预滤波/BRDF LUT、更多灯型 | 参考场景、资源形态、算法范围 |
| 屏幕空间/后处理 | SSAO、Bloom、FXAA/TAA | 深度采样、mip/多分辨率、运动与历史资源 |
| 大场景 | 剔除、实例化、Indirect、Clustered | 真实规模瓶颈与性能基线 |
| Vulkan扩展 | Descriptor Indexing、BDA、Mesh Shader、Ray Tracing | 设备feature/extension请求、支持/启用区分、能力缺失行为 |
| Graph | aliasing、裁剪、Async Compute、多Queue、并行录制 | 状态/所有权正确性与可测收益 |
| 资产工具 | 新格式、压缩纹理、动画、热重载、文件选择器 | 对实验的直接收益与维护成本 |

候选不是交付承诺。纳入时新增或调整稳定REQ、明确依赖/验收；不能把“现代管线”解释为一次实现全部特效。

<a id="maintenance"></a>
## 状态更新规则

用户范围变化先改需求；已实现设计只改 design/README。阶段状态只据实际实现与独立QA推进，操作变更改 usage。一次验收的详细命令/矩阵留构建证据目录，长期入口在归档；当日日志只写一段概述和模块列表。

2026-10-28文档整理未改代码、测试、Shader或构建定义，也未重跑构建/CTest/GPU（该句仅记当日历史）。前一轮架构审阅指出的资源复制、feature语义、Compute/Transfer图像导出等问题未修复，当前设计如实列为限制。
