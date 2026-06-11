# KuEngine 文档中心

本目录按“文档职责”组织。代码发生变化时，应更新对应主题的现有文档，而不是为每次工作创建新的零散记录。

## 目录职责

| 目录 | 内容 | 何时更新 |
|---|---|---|
| `design/` | 当前架构、接口约束、资源规范和阶段性设计 | 模块职责、公共接口或架构边界发生变化时 |
| `logs/` | 按宏观工作主题维护的演进记录 | 架构优化、功能演进、重要重构完成时 |
| `bugs/` | 可复现问题、根因、修复和回归结果 | 发现需要追踪的 Bug，以及 Bug 状态变化时 |
| `usage/` | 示例运行方法、回归步骤和发布说明 | 用户操作、运行参数或验收流程变化时 |

## 文档入口

### 状态说明

| 标记 | 含义 |
|---|---|
| 当前 | 应与当前代码保持同步，可作为实现事实入口 |
| 专项 | 描述某一模块；使用前应同时参考总览和代码 |
| 历史 | 已完成阶段或版本记录，不代表当前待办 |

### 架构与设计

| 文档 | 状态 | 定位 |
|---|---|---|
| [项目架构总览](design/00-overview.md) | 当前 | 项目分层、实际集成边界和版本状态入口 |
| [RHI 层设计](design/01-rhi-layer.md) | 专项 | Vulkan 薄封装与资源生命周期 |
| [RenderPass 与 RenderPipeline](design/02-render-pass.md) | 专项 | Pass 生命周期和渲染调度接口 |
| [日志与调试规范](design/03-logging.md) | 当前 | 日志、Vulkan 错误和 Bug 追踪规则 |
| [Triangle 技术说明](design/04-triangle-example-tech.md) | 历史 | v0.1 最小渲染基线 |
| [UI 层架构](design/05-ui-layer.md) | 专项 | UIOverlay 和 Pass UI 职责 |
| [v0.2 执行计划](design/06-v0.2-execution-plan.md) | 历史 | 已完成的 RenderGraph Alpha 阶段计划 |
| [资源与资产规范](design/07-resource-asset-spec.md) | 当前 | 资源目录和资产命名约束 |
| [glTF 模型加载](design/08-gltf-model-loading.md) | 当前 | 模型、材质和纹理加载链路 |
| [Shader 源码调试模式](design/09-shader-source-debug-mode.md) | 专项 | Shader 编译与 RenderDoc 调试 |

### 工作主题

- [工作日志维护规则](logs/README.md)
- [基础渲染、平台与示例](logs/rendering-platform-and-samples.md)
- [RenderGraph 与渲染调度](logs/render-graph-and-scheduling.md)
- [资产、材质与 PBR](logs/assets-materials-and-pbr.md)

### Bug 与回归

- [Bug 维护规则](bugs/README.md)
- [Bug 报告模板](bugs/template.md)
- [v0.2 回归检查](usage/v0.2-regression-checks.md)

### 使用与发布

- [Triangle 示例](usage/triangle-example.md)
- [Alpha3Pass 示例](usage/alpha3pass-example.md)
- [Mclaren 示例](usage/mclaren-example.md)
- [v0.2 Alpha 发布说明](usage/v0.2-alpha-release-notes.md)

## 代码变更对应关系

| 变更类型 | 必须检查的文档 |
|---|---|
| 公共接口或模块职责变化 | `design/` 对应设计文档、`design/00-overview.md` |
| 架构优化或跨模块重构 | `logs/` 对应主题文档，必要时同步 `CHANGELOG.md` |
| Bug 修复 | `bugs/` 对应 Bug 文档；同步状态、根因、修复与回归结果 |
| 示例操作或参数变化 | `usage/` 对应示例文档 |
| 构建、测试或验收流程变化 | `README.md`、相关 `usage/` 文档 |
| 发布或用户可见能力变化 | `CHANGELOG.md`、发布说明、相关主题日志 |

## 更新原则

1. 先更新已有主题文档；只有出现新的宏观领域时才新建工作日志。
2. 工作日志文件名使用主题，不使用日期；日期作为文档内部时间线标题。
3. 设计文档描述“现在如何工作”，工作日志描述“为何和如何演进”。
4. Bug 文档描述具体问题，不把 Bug 细节混入工作日志。
5. 删除、重命名文档时必须同步修复仓库内引用。
6. 文档中的状态、接口和验证命令应以当前代码为准。
