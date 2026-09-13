# 文档同步规则

修改项目模块前，先阅读 docs/README.md 和 docs/design/README.md，确定对应设计文档。

- 对模块的任何代码修改，都必须核对并同步 design 中受影响的职责、接口、数据流、所有权、约束和图示；同一轮代码工作完成文档更新。
- 纯格式等不改变设计事实的修改可不改正文，但交付时说明已核对。
- design 只描述当前工作区已实现内容；产品目标和功能范围写入 docs/structure/product-requirements.md，未来架构约束写入 target-architecture.md，开发顺序与阶段验收写入 roadmap.md；已实现的宏观架构变化同步 current-architecture.md。
- 新增或调整功能时保留稳定需求编号，先同步产品需求，再调整目标架构与路线图；实现交付后更新需求和阶段状态，不将未验证的实现标记为已验收。
- 迭代摘要按实际日期写入 docs/logs/YYYY-MM-DD.md，只保留一段完成概述和模块更新列表；同日合并。
- usage 维护操作方法，bugs 维护具体问题。历史快照和 structure/archive 不随当前实现重写。
- 移动或重命名文档后修复引用；不得将尚未执行的验证写成通过。

# 多智能体协作规则

项目级子 Agent 在 `.codex/config.toml` 注册，详细角色 Prompt 位于 `docs/prompts`。主 Agent负责拆分与分派，`developer` 负责实现，`qa` 负责独立验收和问题报告，`planner` 负责验收后的文档同步。

- 默认采用单写者模式：Developer 修改代码时实现 Agent是唯一写者；QA 验证期间冻结实现；QA 通过后 Planner 才修改文档。需要并行写入时必须使用独立 Git worktree、分支和构建目录。
- Developer 只能报告 `CODE_COMPLETE`，QA 只能报告 `ACCEPTED`、`REJECTED` 或 `VERIFICATION_BLOCKED`，Planner 只能报告 `DOCUMENTED`；只有主 Agent可以报告 `DONE`。
- QA 不修改实现代码或测试，不用构建成功代替 GPU/功能验收；无法执行的必要检查标记为 `VERIFICATION_BLOCKED` 或未运行。
- Planner 只能基于当前代码与 QA 证据更新状态；即使分工给子 Agent，主 Agent仍负责最终检查本文件中的文档同步要求。
- 子 Agent不得自行提交、推送、删除文件或扩展任务范围，除非当前用户请求和主 Agent任务包明确授权。
