# 当前模块设计

本目录只描述当前代码如何工作。实现计划、版本进度和历史设计保存在 [structure](../structure/README.md)，迭代摘要保存在 [logs](../logs/README.md)。

核对基线2026-10-11：最终C QA ACCEPTED + DOCUMENTED，REQ-01～13首轮范围已验收，WP03/M4已由Lead最终审计闭环（DONE），M5未开始。C为只读审计/验收，未新增Runtime接口：共享RGBA16F/天空/Blend/唯一普通Display，post-UI snapshot、Background/Opaque/Geometry/Blend四独立UBO；strict配置/CLI优先、Fence-safe静态mask、直接MRT八视图/单次编码、UI最后合成；Mclaren实例/slot独立编辑与真正Clear保持既有实现。源码/图示核对与证据边界见[当日日志](../logs/2026-10-11.md)和[回归说明](../usage/regression-checks.md)。

| 文档 | 模块与边界 |
|---|---|
| [00 模块总览](00-overview.md) | 模块依赖与所有权总表 |
| [01 RHI](01-rhi-layer.md) | 设备、资源、上传、同步、命令与 GPU 计时 |
| [02 Render 调度](02-render-pass.md) | Pass 生命周期、Graph 编译、附件执行 |
| [03 日志与诊断](03-logging.md) | spdlog、VK_CHECK 与 Validation |
| [04 示例设计](04-triangle-example-tech.md) | Triangle、Cube、Alpha3Pass、Mclaren、ForwardReuse、GraphResourceProbe、CornellBox 七个示例的当前装配 |
| [05 UI 与性能统计](05-ui-layer.md) | UIOverlay、Pass 控件与统计口径 |
| [06 Core Runtime](06-core-runtime.md) | Engine、Window、Input、帧循环和 resize |
| [07 资产配置](07-resource-asset-spec.md) | 资源目录、Scene/Material JSON 与路径解析 |
| [08 模型加载](08-gltf-model-loading.md) | glTF/GLB 到 CPU Mesh 的转换 |
| [09 构建与 Shader](09-shader-source-debug-mode.md) | CMake、SPIR-V 编译与调试模式 |
| [10 PBR 与 GPU 资源](10-pbr-rendering.md) | Mesh/Texture、Descriptor、动态 UBO、环境渲染 |
| [11 公共 Forward](11-forward-rendering.md) | 公共 API、set 布局、资源所有权、draw 顺序与示例复用 |
| [12 Graph 资源](12-graph-resources.md) | typed handle、descriptor、资源池、状态规划、同步2、Compute/callback、resize/export 生命周期 |
| [13 公共 Deferred](13-deferred-rendering.md) | GBuffer 格式/编码、Geometry/Lighting、共享天空/Blend/HDR显示及验证边界 |

每次修改模块都按 [同步约定](../README.md) 核对对应文档。各文档保留“当前约束”，用于准确描述实现边界；不在这些章节中排任务或列未来阶段。

2026-10-21 新增 CornellBox 公共 Deferred/PBR 消费者：程序化 MeshData 直接上传，复用 SceneDrawSource、四 phase Renderer 与 Graph 附件；公共 Runtime/Shader 未改。REQ-15 独立 QA ACCEPTED，设计见 [04](04-triangle-example-tech.md)，操作与精确证据范围见 [CornellBox](../usage/cornell-box-example.md)。
