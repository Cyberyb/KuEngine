# Bug 维护规则

`docs/bugs/` 用于保存需要复现、分析、修复和回归验证的问题。

## 文件命名

使用稳定的问题主题命名，不使用日期作为主要分类：

```text
<area>-<short-description>.md
```

示例：

```text
swapchain-resize-crash.md
rendergraph-invalid-barrier.md
mclaren-material-binding-error.md
```

若同一主题存在多个独立问题，可附加简短限定词，而不是使用流水号。

## 生命周期

```text
Open -> In Progress -> Resolved
                  \-> Won't Fix
```

状态变化时更新同一文件，不创建新的“修复完成”文档。

## 必填信息

- 首次发现日期和最近更新日期。
- 严重程度、状态和影响模块。
- 可稳定执行的复现步骤。
- 预期行为与实际行为。
- 环境、关键日志或 Validation 信息。
- 根因分析。
- 修复文件和行为变化。
- 构建、测试、手工运行等回归结果。

## 与其他文档的关系

- Bug 的具体过程只记录在 `bugs/`。
- 若修复改变公共架构或接口，需要同步 `design/`。
- 若修复属于重要架构演进，在对应 `logs/` 主题中记录简要影响并链接 Bug。
- 若修复改变用户操作，需同步 `usage/`。
- 用户可见或发布相关修复需同步 `CHANGELOG.md`。
