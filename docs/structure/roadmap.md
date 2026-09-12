# 后续开发目标与计划

更新日期：2026-09-12。此页为待开发目标；当前已实现能力见 [宏观架构](current-architecture.md)，不能将这里的组件名称当作现有 API。

## 总体目标

围绕“快速验证图形算法”，优先补足正确性、资源管理和可复现观测，再扩展渲染能力。公共 Runtime、四示例迁移、可选深度、PBR per-draw UBO、Mclaren 拆分和 Graph Scope 执行已形成基线；GPU 时间已经实现基础 Timestamp，后续是完善而非从零增加。

## 建议顺序

| 优先级 | 目标 | 主要范围 | 完成判断 |
|---|---|---|---|
| P0 | 建立自动 GPU 正确性检查 | RHIInstance、测试、示例 | 收集 Validation 错误并使测试失败；多帧/resize 冒烟可重复运行 |
| P0 | 完善计时与统计契约 | Engine、CommandList、UI | 明确样本帧号，测试 Query 复用/不支持/回绕，按提交状态管理 Query，核对单调时钟与采样范围 |
| P1 | 逐帧资源隔离 | Runtime、PBR、Depth、Query | 2～3 帧并行下动态资源和 Query 按帧槽复用，Validation 与画面回归通过 |
| P1 | 统一同步描述 | CommandList、Graph 执行器 | Stage/Access/Layout 显式表达，迁移 synchronization2，覆盖子资源和 Buffer |
| P1 | Graph 内部资源管理 | ResourceDesc、RenderPipeline | 能声明并分配中间 Image/Buffer，生成真实读写屏障，并完成离屏 → 采样示例 |
| P1 | 拆分通用材质/环境资源 | MclarenRenderResources、Render | 第二个示例可复用材质和环境装配，明确资源所有权，保持 Mclaren 可运行 |
| P1 | 补齐资产语义 | ModelLoader、SceneConfig、Shader | 独立 AO/MR 与 emissive UV 正确，支持逐节点变换/材质的明确数据模型 |
| P2 | 实验观测与复现 | UI、配置、测试 | 每 Pass 计时、曲线、预设与结果导出可用于重复比较 |
| P2 | Compute 与批量上传 | RHI、Graph、Uploader | Compute 示例和上传批处理接入统一同步模型 |
| P2 | API/模块收敛 | 构建、日志、Render Debug UI | 收窄 PUBLIC 依赖、校正调试宏、显式 Shader Stage、分离调试展示 |

当前单帧限制必须在资源按帧隔离完成后才能放开。资源别名、Async Compute、多 Queue、Capture/Replay、动画、压缩纹理等属于后续候选，不预设发布日期。

## 实施约束

每轮代码工作同步更新 [对应模块设计](../design/README.md)，在 logs 当日文件记录宏观完成内容，并据实际完成情况更新本页。design 只写已经落地的接口与行为。

现有 CTest 是 CPU 侧基础；计时、同步和资源管理变更的验收需要真实 GPU 路径。测试操作统一放在 [usage](../usage/regression-checks.md)，详细 Bug 放在 [bugs](../bugs/README.md)。
