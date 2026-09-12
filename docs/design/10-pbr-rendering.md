# 公共 GPU 资源与 PBR 设计

核对日期：2026-09-12。

源码：[GpuMesh](../../src/KuEngine/Render/GpuMesh.h)、[TextureFactory](../../src/KuEngine/Render/TextureFactory.h)、[PBRCommon](../../src/KuEngine/Render/PBRCommon.h)、[PBRRenderer](../../src/KuEngine/Render/PBRRenderer.cpp)、[MclarenRenderResources](../../examples/mclaren/MclarenRenderResources.cpp)。

## 资源所有权

GpuMesh 通过 ResourceUploader 上传 Vertex/Index Buffer，并保存数量和 SubMesh 元数据。TextureFactory 根据 RGBA8、纯色或显式字节/格式创建 RHITexture，再委托上传器完成传输；它不解析 JSON 或模型路径。

MclarenRenderResources 拥有 GpuMesh、Shader/Pipeline、材质/环境/回退纹理、Sampler、Descriptor Pool/Layout/Set、动态 UBO 和 PBRRenderer。PBRRenderer 借用这些资源，只组织逐 draw 绑定与提交；initialize() 当前为空实现。

Material 保存名称与 MaterialConfig，MaterialInstance 关联 Material 和 PBRMaterialBinding。Mclaren 主路径仍使用 PBRMaterialBinding 数组，尚未统一由 MaterialInstance 管理。

## Shader 数据契约

| PBR Pipeline 绑定 | 内容 |
|---|---|
| set 0 / binding 0 | baseColor sampler2D |
| set 0 / binding 1 | normal sampler2D |
| set 0 / binding 2 | ORM sampler2D |
| set 0 / binding 3 | emissive sampler2D |
| set 1 / binding 0 | 动态 PBRFrameUniforms UBO |
| set 2 / binding 0 | 环境 sampler2D |

Skybox Pipeline 使用另一套 set 编号：set 0 是 Frame UBO，set 1 是环境贴图。不能将其与 PBR 编号混用。

PBRPushConstants 提供 MVP、法线变换、颜色、材质因子和 UV 参数；PBRFrameUniforms 包括模型矩阵、相机、逆 ViewProjection、光照、emissive 和 alpha。C++ 布局与 GLSL 声明必须同步。资源创建时校验 maxPushConstantsSize；此数据块较大，不保证所有 Vulkan 设备均能创建 Mclaren Pipeline。

## per-draw UBO

```text
alignedStride = 对齐 sizeof(PBRFrameUniforms) 到 minUniformBufferOffsetAlignment
offset 0                 → Skybox Frame
offset alignedStride     → PBR draw 0
offset 2 × alignedStride → PBR draw 1
...
```

PBRRenderer 校验 drawItems 与逐 draw Push/Frame 数量及缓冲容量，映射一次、写入所有 draw 的独立区域、flush/unmap，然后在每次 drawIndexed 前绑定对应 dynamicOffset。indexCount=0 的条目不绘制。

该结构避免循环覆盖同一 UBO 区域。当前所有帧仍共享一个缓冲，由 Runtime 单帧 Fence 顺序保证复用安全，未提供多帧分区。

## 材质与环境

Material JSON 可覆盖因子、alpha 和部分纹理来源/色彩空间/UV 选择；默认纹理用于缺失贴图。BaseColor/Emissive 使用颜色纹理路径，Normal/ORM 使用线性数据路径。

Mclaren 的 PBR Pipeline 开启深度测试/写入，颜色/深度格式和 CompareOp 来自 Runtime；当前固定 cullMode=NONE。JSON BLEND 决定 Pipeline 混合，MASK 在 Shader discard。没有逐透明物体排序或按每个 glTF 材质切换 Pipeline。

Skybox 采用全屏三角形，在同一 Mclaren Scope 中先画，关闭深度测试/写入；PBR 随后绘制。环境读取 equirectangular HDR，失败时回退白纹理；目前不是完整预滤波 IBL 管线。

## 当前限制

- RenderResources 仍位于示例，材质/环境装配没有公共工厂。
- ORM 假设与 emissive UV 限制见 [模型加载](08-gltf-model-loading.md)。
- 外部贴图路径、独立节点材质/变换、资源缓存、异步上传未接通。
- 统计使用 CommandList::drawIndexed；原始 Vulkan draw 无法自动计数。
- tests/core/test_pbr.cpp 验证 CPU 侧辅助逻辑，尚无自动多材质图像回归。
