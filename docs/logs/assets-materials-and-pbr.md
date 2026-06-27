# 资产、材质与 PBR

本主题记录资源目录、场景配置、glTF/GLB、材质、纹理、PBR 和环境渲染的演进。

## 2026-06-11：GPU 上传、纹理与网格职责下沉

### 背景与目标

`MclarenPass` 原本直接负责 Staging Buffer、瞬时 CommandPool、Buffer/Image Copy、
图像布局转换和 Queue 同步提交。该逻辑属于通用 RHI 能力，并且每张纹理都会重复创建
和销毁 CommandPool。

### 完成内容

- 新增 `RHI::ResourceUploader`，统一管理瞬时 CommandPool。
- 提供 Buffer 上传和 2D Texture 上传接口。
- 集中处理 Staging Buffer 映射、刷新、拷贝和同步提交。
- 集中处理纹理的 `UNDEFINED -> TRANSFER_DST -> SHADER_READ_ONLY` 布局转换。
- Mclaren 顶点/索引缓冲改为 `TRANSFER_DST` 目标缓冲，通过 Staging 上传。
- 普通纹理、纯色回退纹理和 HDR 环境纹理改用公共上传器。
- 移除 `MclarenPass` 内部的 `submitImmediate` 和重复上传命令。
- 新增 `TextureFactory`，统一创建并上传普通纹理、纯色纹理和 HDR 像素纹理。
- 新增 `GpuMesh`，统一拥有 Vertex/Index Buffer、数量统计和 SubMesh 元数据。
- `MclarenPass` 不再直接创建静态 Vertex/Index Buffer 或 `RHITexture`。

### 架构影响

- 资产层继续决定像素来源和纹理格式。
- RHI 层只负责将已准备好的字节上传到指定 GPU 资源。
- `MclarenPass` 仍负责选择资产来源、材质覆盖和 Descriptor 装配。
- 下一阶段将材质纹理选择、默认纹理、Descriptor Pool/Layout 和材质绑定集中到
  `PBRMaterialFactory`。

### 关联

- 代码：`src/KuEngine/RHI/ResourceUploader.*`
- 代码：`src/KuEngine/Render/TextureFactory.*`
- 代码：`src/KuEngine/Render/GpuMesh.*`
- 代码：`examples/mclaren/MclarenPass.*`

### 验证

- `cmake --build --preset debug` 全量构建通过。
- `ctest --preset debug --output-on-failure` 通过。
- `MclarenApp` 后台启动冒烟通过，完成模型、材质、环境纹理和 Pipeline 初始化。
- 冒烟窗口内无 `ERROR`、`CRITICAL` 或 Vulkan Validation Error。

## 2026-06-11：PBR 公共层拆分

### 背景与目标

`MclarenPass` 同时承担场景配置、模型与纹理上传、材质绑定、PBR 绘制和天空盒等职责。当前开始将可复用能力下沉到公共渲染层。

### 完成内容

- 引入 `PBRCommon`，集中 PBR Push Constants、Frame Uniforms 和材质绑定结构。
- 引入 `PBRRenderer`，承接 Pipeline、Descriptor、Vertex/Index Buffer 绑定和 indexed draw。
- 增加 `Material` 与 `MaterialInstance` 类型骨架。

### 当前边界

- 纹理上传、Descriptor 创建、环境贴图和大部分材质装配仍位于 `MclarenPass`。
- 公共材质实例尚未完全替换示例内部绑定数据。
- 后续应优先拆分 PBRMaterialFactory、EnvironmentMap 和 SkyboxRenderer。

### 关联

- 代码：`examples/mclaren/MclarenPass.*`
- 代码：`src/KuEngine/Render/PBRCommon.*`
- 代码：`src/KuEngine/Render/PBRRenderer.*`
- 设计：[项目架构总览](../design/00-overview.md)

## 2026-04-19：场景与材质配置闭环

### 完成内容

- Mclaren 示例接入 scene/material 配置读取。
- 场景 JSON 驱动相机 position、target、up、FOV、near 和 far。
- 场景 JSON 驱动基础光照 direction、color 和 intensity。
- 材质 JSON 驱动全局 baseColorFactor，并保留 glTF 材质回退。
- 新增 `AssetConfig` 模块。
- 新增资产配置解析回归测试。
- 同步 Mclaren 使用文档、glTF 设计文档、架构总览和 Changelog。

### 验证

- `cmake --build --preset debug --target MclarenApp` 通过。
- `ctest --preset debug --output-on-failure` 通过。
- `examples/mclaren/run_mclaren.bat` 通过。

### 后续方向

- material JSON 完整接管纹理绑定。
- 多节点场景装配。
- 扩展 PBR 工作流和完整 TBN 路径。
