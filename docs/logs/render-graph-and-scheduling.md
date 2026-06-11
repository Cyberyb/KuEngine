# RenderGraph 与渲染调度

本主题记录 RenderPass、RenderPipeline、RenderGraph、资源依赖、屏障计划及多 Pass 示例的演进。

## 2026-04-15：v0.2 Alpha 发布收尾

### 完成内容

- 完成功能、工程和可观测性门禁复核。
- 更新执行计划和总览状态，确认阶段 0-6 全部完成。
- 新增 v0.2 Alpha 发布说明并同步 `CHANGELOG.md`。

### 验证

- `cmake --build --preset debug` 通过。
- `ctest --preset debug` 通过。
- `examples/alpha3pass/run_alpha3pass.bat` 通过。

### 结论

v0.2 Alpha 已形成可继续迭代的 RenderGraph 基线。

## 2026-04-14：RenderGraph Alpha 阶段 0-5

### 阶段 0：范围冻结

- 冻结 In Scope / Out of Scope。
- 定义功能、工程和可观测性门禁。
- 建立阶段 1-6 执行计划与风险应对。
- 产出 `docs/design/06-v0.2-execution-plan.md`。

### 阶段 1：最小数据模型

- 新增 Pass 节点、资源句柄、读写声明和执行顺序缓存。
- 新增 `RenderGraphBuilder::createResource/importExternal/read/write`。
- `RenderPass` 增加 `setup(RenderGraphBuilder&)`，保留旧 `setup()` 兼容桥接。
- `RenderPipeline::compile()` 接入图声明路径。
- TrianglePass 和 CubePass 声明写入 `SwapChainColor`。

### 阶段 2：依赖编译器

- 从资源冲突建立依赖边。
- 新增 `dependsOn(passName)` 显式依赖。
- 使用拓扑排序生成执行顺序。
- 增加缺失依赖和循环依赖检测。
- 生成 RAW、WAR、WAW 屏障计划。
- 增加 RenderGraph 单元测试。

### 阶段 3：执行器对接 RHI

- `RenderPipeline` 开始消费屏障计划。
- 增加外部图像和当前布局绑定。
- 通过 `CommandList::imageBarrier` 发射图像屏障。
- Triangle/Cube 移除重复手写屏障函数。

验证：

- `cmake --preset vs2022-vcpkg` 通过。
- `cmake --build --preset debug` 通过。
- `ctest --preset debug` 通过。

### 阶段 4：三 Pass Alpha 场景

- 新增 `Alpha3PassApp`。
- 建立 `Background -> MainTriangle -> Accent` 显式依赖链。
- 三个 Pass 均提供颜色、偏移和缩放 UI。
- 新增独立 Shader、运行脚本和使用文档。

验证：

- `cmake --build --preset debug --target Alpha3PassApp` 通过。
- `ctest --preset debug` 通过。

### 阶段 5：可观测性与测试

- 增加 `RenderGraph Debug` 面板。
- 展示编译摘要、依赖边、屏障计划和最近一帧执行摘要。
- 记录屏障 applied/skipped 状态及原因。
- 补充 RAW/WAR/WAW 和 external resource 测试。
- 新增 `docs/usage/v0.2-regression-checks.md`。

### 当前边界

- RenderGraph 负责逻辑依赖、拓扑排序和部分外部图像屏障。
- 内部 Vulkan 资源创建、生命周期分析、资源别名和完整状态跟踪仍未实现。
- 完整帧循环仍主要由示例程序装配。
