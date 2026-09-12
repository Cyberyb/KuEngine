# 项目架构与开发方向

这里放项目的宏观现状、未来目标和开发计划。模块接口与具体实现以 [design](../design/README.md) 为准。

| 文档 | 用途 |
|---|---|
| [产品需求](product-requirements.md) | 产品定位、功能范围、稳定需求编号、当前状态与验收口径；新增或调整功能先改这里 |
| [目标架构](target-architecture.md) | 未来模块边界、RDG 风格资源契约、双渲染路径与 Vulkan 原生扩展原则 |
| [当前宏观架构](current-architecture.md) | 当前能力、所有权、主数据流与测试边界；随架构变更更新 |
| [开发路线图](roadmap.md) | M0～M5 的交付顺序、依赖、工作范围和阶段验收条件 |
| [Structure_725](Structure_725.md) | 2026-07-25 历史快照，用于迁移前后对比 |
| [Structure_726](Structure_726.md) | 2026-07-26 历史快照，不覆盖后续统计等改动 |
| [历史资料](archive/README.md) | 旧计划、发布/回归记录、原架构 PNG、PDF 与未完成草稿 |

product-requirements、target-architecture、current-architecture 和 roadmap 是持续更新入口，分别回答“要做什么”“目标如何组织”“目前实际怎样”“按什么顺序交付”。目标不等于现状，需求状态不在多个文档重复维护。

日期快照与 archive 保留当时观点，不作为当前实现依据，也不在每次模块修改时重写。原有架构 PNG 保留在 archive，仍可用于历史对比。

原有 PDF 是历史导出物；更新 Markdown 不会自动刷新 PDF。
