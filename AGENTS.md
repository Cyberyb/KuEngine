# 文档同步规则

修改项目模块前，先阅读 docs/README.md 和 docs/design/README.md，确定对应设计文档。

- 对模块的任何代码修改，都必须核对并同步 design 中受影响的职责、接口、数据流、所有权、约束和图示；同一轮代码工作完成文档更新。
- 纯格式等不改变设计事实的修改可不改正文，但交付时说明已核对。
- design 只描述当前工作区已实现内容；未来计划和目标写入 docs/structure/roadmap.md，宏观架构变化同步 current-architecture.md。
- 迭代摘要按实际日期写入 docs/logs/YYYY-MM-DD.md，只保留一段完成概述和模块更新列表；同日合并。
- usage 维护操作方法，bugs 维护具体问题。历史快照和 structure/archive 不随当前实现重写。
- 移动或重命名文档后修复引用；不得将尚未执行的验证写成通过。
