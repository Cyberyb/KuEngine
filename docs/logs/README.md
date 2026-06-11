# 工作日志维护规则

`docs/logs/` 用于记录代码架构和主要能力的演进过程。日志按宏观工作主题组织，不按日期拆分文件。

## 当前主题

| 主题文档 | 覆盖范围 |
|---|---|
| [基础渲染、平台与示例](rendering-platform-and-samples.md) | Vulkan 基线、SwapChain、UI、基础示例与通用平台能力 |
| [RenderGraph 与渲染调度](render-graph-and-scheduling.md) | RenderPass、RenderPipeline、RenderGraph、屏障和多 Pass 调度 |
| [资产、材质与 PBR](assets-materials-and-pbr.md) | 资源目录、场景配置、glTF、材质、纹理、PBR 和环境渲染 |

只有当新增工作无法合理归入这些宏观领域时，才创建新的主题文档。

## 单次更新格式

在对应主题文档的时间线顶部追加一节：

```markdown
## YYYY-MM-DD：工作主题

### 背景与目标

说明为什么进行本次工作。

### 完成内容

- 关键代码和架构变化。

### 验证

- 执行的构建、测试和运行验证。

### 影响与后续

- 当前边界、风险和下一步。

### 关联

- 代码：`path/to/file`
- 设计：`docs/design/...`
- Bug：`docs/bugs/...`（如有）
```

## 维护要求

- 架构重构完成后必须更新对应主题日志。
- 内容以决策、边界和验证结果为主，避免逐文件流水账。
- 同一工作跨越多天时，在同一主题文档中追加新的日期节点。
- 已被后续实现推翻的结论不直接删除，应标记为“已替代”并链接新记录。
- Bug 修复的详细过程写入 `docs/bugs/`；主题日志只记录具有架构意义的修复。
