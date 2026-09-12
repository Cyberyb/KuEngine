# 当前模块设计

本目录只描述当前代码如何工作。实现计划、版本进度和历史设计保存在 [structure](../structure/README.md)，迭代摘要保存在 [logs](../logs/README.md)。

核对基线：2026-09-12 工作区，含公共 Runtime、Graph 执行器、PBR 动态 UBO，以及未提交的 Draw Call / CPU / GPU 统计改动。

| 文档 | 模块与边界 |
|---|---|
| [00 模块总览](00-overview.md) | 模块依赖与所有权总表 |
| [01 RHI](01-rhi-layer.md) | 设备、资源、上传、同步、命令与 GPU 计时 |
| [02 Render 调度](02-render-pass.md) | Pass 生命周期、Graph 编译、附件执行 |
| [03 日志与诊断](03-logging.md) | spdlog、VK_CHECK 与 Validation |
| [04 示例设计](04-triangle-example-tech.md) | Triangle、Cube、Alpha3Pass、Mclaren 的当前装配 |
| [05 UI 与性能统计](05-ui-layer.md) | UIOverlay、Pass 控件与统计口径 |
| [06 Core Runtime](06-core-runtime.md) | Engine、Window、Input、帧循环和 resize |
| [07 资产配置](07-resource-asset-spec.md) | 资源目录、Scene/Material JSON 与路径解析 |
| [08 模型加载](08-gltf-model-loading.md) | glTF/GLB 到 CPU Mesh 的转换 |
| [09 构建与 Shader](09-shader-source-debug-mode.md) | CMake、SPIR-V 编译与调试模式 |
| [10 PBR 与 GPU 资源](10-pbr-rendering.md) | Mesh/Texture、Descriptor、动态 UBO、环境渲染 |

每次修改模块都按 [同步约定](../README.md) 核对对应文档。各文档保留“当前约束”，用于准确描述实现边界；不在这些章节中排任务或列未来阶段。
