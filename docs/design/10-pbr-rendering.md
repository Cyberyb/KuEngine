# 公共 GPU 资源与 PBR 设计

核对日期：2026-10-07（M3-WP03 Shader 接口调用核对）。

源码：[GpuModelAsset](../../src/KuEngine/Render/GpuModelAsset.h)、[PBRResources](../../src/KuEngine/Render/PBRResources.h)、[PBRRenderer](../../src/KuEngine/Render/PBRRenderer.cpp)、[MclarenRenderResources](../../examples/mclaren/MclarenRenderResources.cpp)。

## 资源所有权

GpuModelAsset 拥有一个上传后的 GpuMesh；TextureFactory 根据 RGBA8、RGBA float、纯色或显式字节/格式创建 RHITexture，再委托上传器完成传输；它不解析 JSON 或模型路径。`MaterialGpuResources`（PBRMaterialResources 的兼容别名）窄持有五类材质纹理、fallback、Sampler、descriptor layout/pool/set 和 binding 快照；PBREnvironmentResources 窄持有 HDR 环境纹理、Sampler 与其 descriptor 资源。公共 Forward 的完整契约见 [11](11-forward-rendering.md)。

`MclarenRenderResources`、`PBRRenderer` 及其旧 shader 文件仍可留在源码树作兼容遗留，但当前 Mclaren 不构建或运行该路径；公共 `ForwardProgram`/`ForwardRenderer` 是实际 Pipeline、frame/draw UBO、Skybox 与 PBR/Unlit draw 的所有者边界，详见 [公共 Forward](11-forward-rendering.md)。

模型替换创建完整 `MclarenModelState` 候选，包含 Scene/fit、按 MeshHandle 的 GpuModelAsset、材质 variant 与公共 Forward draw；新 SubMesh 数量据此决定 renderer 容量和 expected draw/vertex 统计。环境替换创建独立 `MclarenEnvironmentState` 候选，仅替换 HDR 环境资源并由既有公共 Forward 重新绑定；它不重建模型、材质、相机或模型统计。候选失败不会交换指针、路径、generation 或统计；`VK_ERROR_DEVICE_LOST` 不是可恢复保留情形，而是向上失败。

Material 保存名称与 MaterialConfig，MaterialInstance 关联 Material 和 PBRMaterialBinding。Mclaren 主路径仍使用 PBRMaterialBinding 数组，尚未统一由 MaterialInstance 管理。

## Shader 数据契约

| PBR Pipeline 绑定 | 内容 |
|---|---|
| set 0 / binding 0 | baseColor sampler2D |
| set 0 / binding 1 | normal sampler2D |
| set 0 / binding 2 | metallic-roughness sampler2D |
| set 0 / binding 3 | emissive sampler2D |
| set 1 / binding 0 | 动态 PBRFrameUniforms UBO |
| set 2 / binding 0 | 环境 sampler2D |

Skybox Pipeline 使用另一套 set 编号：set 0 是 Frame UBO，set 1 是环境贴图。不能将其与 PBR 编号混用。

公共 Forward 不再使用旧 PBRPushConstants/PBRFrameUniforms 主路径：固定 set 0～3，以 frame UBO、动态 draw UBO 和小 skybox push constants 传递数据；它已调用显式 path/stage/entry 的 graphics shader 描述，Shader 接口迁移不改变材质或资源语义。旧 Mclaren PBR shader/资源文件可保留在树中，但不构建或运行。

## per-draw UBO

```text
alignedStride = 对齐 sizeof(PBRFrameUniforms) 到 minUniformBufferOffsetAlignment
offset 0                 → Skybox Frame
offset alignedStride     → PBR draw 0
offset 2 × alignedStride → PBR draw 1
...
```

PBRRenderer 校验 drawItems 与逐 draw Push/Frame 数量及缓冲容量，映射一次、写入所有 draw 的独立区域、flush/unmap，然后在每次 drawIndexed 前绑定对应 dynamicOffset。PBRDrawItem 通过 materialIndex 索引内部复制的 material binding 快照，不借用调用方 vector；空材质数组、越界索引和零索引项回退到默认 material/跳过无索引 draw，避免 descriptor 越界。indexCount=0 的条目不绘制。

该结构避免循环覆盖同一 UBO 区域。当前所有帧仍共享一个缓冲，由 Runtime 单帧 Fence 顺序保证复用安全，未提供多帧分区。

## 材质与环境

Material JSON 可覆盖因子、alpha 和部分纹理来源/色彩空间/UV 选择；默认纹理用于缺失贴图。BaseColor/Emissive 使用颜色纹理路径，Normal/ORM 使用线性数据路径。

Mclaren 的 PBR Pipeline 开启深度测试/写入，颜色/深度格式和 CompareOp 来自 Runtime；当前固定 cullMode=NONE。JSON BLEND 决定 Pipeline 混合，MASK 在 Shader discard。没有逐透明物体排序或按每个 glTF 材质切换 Pipeline。

Skybox 采用全屏三角形，在同一 Mclaren Scope 中先画，关闭深度测试/写入；PBR 随后绘制。环境读取 equirectangular HDR，失败时回退白纹理；目前不是完整预滤波 IBL 管线。运行时 HDR 替换采用新候选上传成功后发布，而不是在已使用 descriptor 上原地改写。

## 当前限制

- 公共资产只有窄所有权类；Mclaren 有同步替换但尚无场景级资源缓存、Registry、异步/文件监听替换或公共资产工厂。
- 没有完整 IBL、OIT、色度正确/线性 HDR 统一输出、Deferred 或多帧资源 retire；Graph-owned SceneColor/Depth 与 Display 已属于当前公共 Forward 实现。
- 外部贴图路径、独立节点材质/变换、资源缓存、异步上传、ShadingModel/Unlit 与公共 Forward 未接通。
- 统计使用 CommandList::drawIndexed；原始 Vulkan draw 无法自动计数。
- tests/core/test_pbr.cpp 验证 CPU 侧辅助逻辑，尚无自动多材质图像回归。
