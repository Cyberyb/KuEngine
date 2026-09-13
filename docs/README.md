# KuEngine 文档中心

文档按职责维护。当前设计以工作区源码为准，包含尚未提交的实现；历史快照只用于对比。最近一次全量核对：2026-09-12。

## 目录职责

| 目录 | 内容 | 维护方式 |
|---|---|---|
| [design](design/README.md) | 各模块当前职责、接口、所有权、数据流和实现约束 | 模块代码发生任何修改，都必须检查并同步对应设计说明 |
| [logs](logs/README.md) | 每日迭代简记 | 一天一个 `YYYY-MM-DD.md`，一段完成概述加模块更新列表 |
| [structure](structure/README.md) | 产品需求、目标架构、当前宏观架构和开发路线 | 需求变化先改产品需求，再同步目标架构与路线图；已实现架构变化更新现状 |
| [usage](usage/README.md) | 构建、运行、调试和回归操作 | 命令、路径、交互或检查方法变化时同步 |
| [bugs](bugs/README.md) | 具体问题的复现、根因和修复记录 | 在同一个问题文件内持续更新状态 |
| [prompts](prompts/README.md) | 多智能体角色工作方式、交接格式和阶段 Prompt | 固定角色变化时更新角色 Prompt；阶段计划变化时同步相应阶段 Prompt |

## 阅读入口

- 了解项目：[当前宏观架构](structure/current-architecture.md)。
- 明确产品方向：[产品需求](structure/product-requirements.md) 与 [目标架构](structure/target-architecture.md)。
- 阅读模块：[设计索引](design/README.md)。
- 查看下一步：[开发路线图](structure/roadmap.md)。
- 运行项目：[使用说明索引](usage/README.md)。
- 回顾迭代：[按日期的日志](logs/README.md)。
- 组织多智能体开发：[团队协作方式](structure/team-workflow.md) 与 [Prompt 索引](prompts/README.md)。
- 对比历史：[架构快照与历史资料](structure/archive/README.md)。

## 代码与文档同步约定

每次修改模块代码，都在同一轮工作中核对对应 design 文档，修订受影响的职责、流程、参数、约束、图示和源码链接。即使公共接口没有变化，内部行为变化也需要反映；若修改确实不影响设计事实，明确记录已核对，无需制造无意义改写。

| 修改范围 | 必须核对的 design 文档 |
|---|---|
| Core/Engine、Window、Input | [Core Runtime](design/06-core-runtime.md) |
| RHI、同步、上传、命令统计、Timestamp Query | [RHI](design/01-rhi-layer.md) |
| RenderPass、RenderContext、RenderGraph、RenderPipeline | [渲染调度](design/02-render-pass.md) |
| Core/Log、Vulkan 错误处理 | [日志与诊断](design/03-logging.md) |
| UIOverlay、性能统计口径 | [UI 与统计](design/05-ui-layer.md) |
| AssetConfig、资源路径、JSON | [资产配置](design/07-resource-asset-spec.md) |
| ModelLoader、glTF 数据转换 | [模型加载](design/08-gltf-model-loading.md) |
| GpuMesh、TextureFactory、Material、PBR、Mclaren 资源 | [PBR 与 GPU 资源](design/10-pbr-rendering.md) |
| 四个示例的装配、Pass 和相机 | [示例设计](design/04-triangle-example-tech.md) |
| CMake、Shader、启动与编译脚本 | [构建与 Shader](design/09-shader-source-debug-mode.md) |

跨模块变更还需核对 design 总览和 structure 宏观架构；未来任务只进入 structure，完成记录只进入当日日志。移动或重命名文档时同步修复引用。

新增或调整产品功能时，先更新 structure/product-requirements.md 中的稳定需求编号、范围和验收口径，再调整目标架构及路线图依赖。代码交付后同步需求状态和阶段状态；尚未落地的目标不得写成 design 中的当前事实。

这些是仓库维护约定，并非已经实现的自动文档生成或 CI 检查。
