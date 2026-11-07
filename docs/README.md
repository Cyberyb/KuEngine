# KuEngine 文档

KuEngine 是基于 Vulkan 1.3 的小型图形实验框架。当前有公共 Runtime、Graphics/Compute RenderGraph、Forward/Deferred、模型/HDR 输入和八个示例；支持 1/2 帧在途，默认 2 帧。

## 按目的阅读

| 你要做什么 | 从这里开始 |
|---|---|
| 编译并看画面 | [开发与运行](usage/README.md#build)，再选一个[示例](usage/examples.md) |
| 看懂项目、修改算法 | [当前架构](design/README.md)，再读[新增渲染特性](usage/README.md#feature) |
| 决定下一步做什么 | [路线图](structure/roadmap.md)；需要确认范围时查[需求](structure/product-requirements.md) |

日常只维护下面 **6 份文档**，不需要全部通读：

| 文档 | 唯一职责 |
|---|---|
| 本页 | 阅读入口 |
| [当前架构](design/README.md) | 已实现的模块、所有权、数据流和限制 |
| [开发与运行](usage/README.md) | 构建、调试、接入算法和验证方法 |
| [示例手册](usage/examples.md) | 八个示例的专用参数与操作 |
| [产品需求](structure/product-requirements.md) | 稳定 REQ 编号、范围和验收口径 |
| [路线图](structure/roadmap.md) | 阶段状态、后续顺序和未来架构约束 |

字段与实现以文中的源码链接为准。进度只在路线图集中维护，历史测试流水不重复粘贴到每篇说明。

## 需要追溯时

[历史与验收记录](structure/archive/README.md) 保留旧架构、每日简记、任务包及 QA 证据入口。整理前全文已按原始字节备份，可恢复旧路径与未提交内容。

维护约定见根目录 [AGENTS.md](../AGENTS.md)。Agent 专用角色说明在 [.codex/prompts](../.codex/prompts)，不属于日常阅读材料。
