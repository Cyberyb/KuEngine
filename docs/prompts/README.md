# 多智能体 Prompt 索引

本目录保存 KuEngine 多智能体协作中的长指令、任务模板和阶段 Prompt。项目级角色由 [`.codex/config.toml`](../../.codex/config.toml) 注册；仓库共同规则由 [`AGENTS.md`](../../AGENTS.md) 定义；开发路线与进度仍维护在 [structure](../structure/README.md)。

| Prompt | 使用者 | 用途 |
|---|---|---|
| [主 Agent](lead.md) | 根 Agent / Lead | 建立任务包、分派角色、控制验收门禁与最终集成 |
| [实现 Agent](developer.md) | `developer` | 完成一个范围明确的代码工作包并提交实现交接 |
| [验收 Agent](qa.md) | `qa` | 独立检查 diff、执行验证并给出验收结论 |
| [文档 Agent](planner.md) | `planner` | 根据实现与验收证据同步当前设计和管理文档 |

这里的 Prompt 不会自行创建 Agent。主 Agent 在创建或续派子 Agent 时，应同时传入任务编号、任务包和所需角色；角色 TOML 提供稳定身份，本目录提供可审阅的详细工作方法。

后续 M0～M5 的阶段 Prompt 放在 `docs/prompts/stages/`，任务包模板放在 `docs/prompts/task-packet.md`。在恢复阶段实施计划前不提前创建空文件。

## 信息优先级

发生冲突时按以下顺序处理：当前用户指令 → 平台与安全规则 → 当前任务包 → 根目录 `AGENTS.md` → 角色配置与本目录角色 Prompt → structure 中的长期计划。发现冲突时报告主 Agent，不自行降低验收标准或扩大任务范围。

## 交接状态

```text
READY → IMPLEMENTING → CODE_COMPLETE → VERIFYING
      → REJECTED ────────────────────→ IMPLEMENTING
      → VERIFICATION_BLOCKED
      → ACCEPTED → DOCUMENTING → DOCUMENTED → DONE
```

`CODE_COMPLETE` 由实现 Agent报告；`ACCEPTED`、`REJECTED` 或 `VERIFICATION_BLOCKED` 由 QA 报告；`DOCUMENTED` 由文档 Agent报告；只有主 Agent可以报告 `DONE`。
