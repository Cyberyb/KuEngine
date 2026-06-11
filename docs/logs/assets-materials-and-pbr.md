# 资产、材质与 PBR

本主题记录资源目录、场景配置、glTF/GLB、材质、纹理、PBR 和环境渲染的演进。

## 2026-06-11：PBR 公共层拆分进行中

### 背景与目标

`MclarenPass` 同时承担场景配置、模型与纹理上传、材质绑定、PBR 绘制和天空盒等职责。当前开始将可复用能力下沉到公共渲染层。

### 当前变化

- 引入 `PBRCommon`，集中 PBR Push Constants、Frame Uniforms 和材质绑定结构。
- 引入 `PBRRenderer`，承接 Pipeline、Descriptor、Vertex/Index Buffer 绑定和 indexed draw。
- 增加 `Material` 与 `MaterialInstance` 类型骨架。

### 当前边界

- 纹理上传、Descriptor 创建、环境贴图和大部分材质装配仍位于 `MclarenPass`。
- 公共材质实例尚未完全替换示例内部绑定数据。
- 后续应优先拆分 ResourceUploader、TextureFactory、GpuMesh 和 PBRMaterialFactory。

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
