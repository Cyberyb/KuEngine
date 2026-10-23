# 公共 GPU 资源与 PBR 设计

核对日期：2026-10-11（C已验收既有共享资源/实例编辑/环境清空，无新实现）。

源码：[GpuModelAsset](../../src/KuEngine/Render/GpuModelAsset.h)、[PBRResources](../../src/KuEngine/Render/PBRResources.h)、[ForwardRenderer](../../src/KuEngine/Render/ForwardRenderer.cpp)、[共享材质采样](../../resources/shaders/common/material_sample.glsl)。

## 资源所有权

GpuModelAsset 拥有一个上传后的 GpuMesh；TextureFactory 根据 RGBA8、RGBA float、纯色或显式字节/格式创建 RHITexture，再委托上传器完成传输；它不解析 JSON 或模型路径。`MaterialGpuResources`（PBRMaterialResources 的兼容别名）窄持有五类材质纹理、fallback、Sampler、descriptor layout/pool/set 和 binding 快照；PBREnvironmentResources 窄持有 HDR 环境纹理、Sampler 与其 descriptor 资源。公共 Forward 的完整契约见 [11](11-forward-rendering.md)。

`MclarenRenderResources`、`PBRRenderer` 及其旧 shader 文件仍可留在源码树作兼容遗留，但当前 Mclaren 不构建或运行该路径；公共 `ForwardProgram`/`ForwardRenderer` 是实际 Pipeline、frame/draw UBO、Skybox 与 PBR/Unlit draw 的所有者边界，详见 [公共 Forward](11-forward-rendering.md)。

模型替换创建完整 `MclarenModelState` 候选，包含 Scene/fit、按 MeshHandle 的 GpuModelAsset、材质 variant 与公共 Forward draw；新 SubMesh 数量据此决定 renderer 容量和 expected draw/vertex 统计。环境替换创建独立 `MclarenEnvironmentState` 候选，仅替换 HDR 环境资源并由既有公共 Forward 重新绑定；它不重建模型、材质、相机或模型统计。候选失败不会交换指针、路径、generation 或统计；`VK_ERROR_DEVICE_LOST` 不是可恢复保留情形，而是向上失败。

Material 保存名称与 MaterialConfig，MaterialInstance 关联 Material 和 PBRMaterialBinding；这些兼容接口不是 Mclaren 当前 draw 装配主路径。主路径通过 SceneInstance/ResolvedMaterial、MaterialGpuVariantPlan 和 ForwardDraw 接入公共 Renderer。

## Shader 数据契约

当前主路径采用公共 Forward set0 材质五 sampler、set1 frame UBO、set2 环境、set3 dynamic draw UBO。Geometry 复用同一布局，Lighting 使用 MRT/frame/environment；见 [Forward](11-forward-rendering.md) 与 [Deferred](13-deferred-rendering.md)。以下旧 PBR Pipeline 表及 UBO 布局仅说明树中未构建遗留代码，不是当前运行绑定。

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

本节布局属于旧 PBRRenderer。当前 ForwardRenderer 独立 frame UBO、对齐 ForwardDrawUniforms 动态区域，仍由单帧 Fence 保证复用安全；不把旧 Skybox Frame offset 布局用于公共路径。

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

当前公共 Pipeline key 按 ShadingModel/AlphaMode/doubleSided 分派，Opaque/Mask 深度测试并写入，Mask discard；Blend 使用混合、depth test/no write，按对象中心远到近稳定排序。doubleSided 控制 cull。Deferred Geometry 仅支持 Opaque/Mask，明确拒绝 Blend；RGBA16F 四 MRT 格式固定，Depth/CompareOp 由 Runtime 选择。

Skybox全屏三角形位于独立SceneBackground Scope，关闭深度测试/写入，两路径复用；天空可见与环境照明贡献独立。HDR新候选完整上传后发布，不原地改写活跃descriptor。真正`ClearEnvironment`是独立请求，创建同environment layout的有效1×1 RGBA32F(0,0,0,1)完整纹理/sampler/descriptor GPU候选；下一Fence-safe update成功后交换state、清空active path、递增environment generation、关闭sky/env，再reset旧GPU所有权。失败保留旧发布状态；Clear后两路径/GBuffer/resize/recompile仍可用，再Load从空环境恢复sky/env。不会删除用户HDR文件，不置nullptr或悬空descriptor。

`MclarenEnvironmentState`析构先`resources.reset()`，后记录局部寿命token released与`gpu_reset=1`；不是只比较裸指针，也不是M5 profiler/物理OOM测试。初始SceneData.environment只保留导入元数据，activepublishedHDR以Source/controller为准。完整预滤波IBL未实现。

实例slot/TRS编辑复用GPU mesh/material resources及descriptor：`resolveInstanceMaterial`在glTF+instance override后应用scalar-only slot，Source重算候选DrawRecord material/instanceWorld并准备Forward/Geometry有限keys与四phase容量，全部成功才发布SceneData实例和draw。不重新上传几何、贴图或HDR；各phase独立UBO仍在执行时写入，导入fit/相机保持。

共享材质/光照schema不变，正常线性RGBA16F→Display曝光/Reinhard/sRGB。WP03-A各Background/Opaque/Geometry/Blend四套独立Renderer UBO加Lighting帧UBO，模型候选确保两surface分支/Blend容量，正常CPU mask切换不上传资产；ready recompile保留state。debug只直接MRT映射编码、不用材质曝光/tone/Blend颜色，GPU资产仍Source长期所有权。

## 当前限制

- 公共资产只有窄所有权类；Mclaren 有同步替换但尚无场景级资源缓存、Registry、异步/文件监听替换或公共资产工厂。
- Graph-owned线性HDR/天空/Blend、路径/GBuffer、Mclaren实例slot编辑/真正Clear首轮已获最终C QA ACCEPTED；不是通用编辑器，最终M4 Lead闭环完成。完整IBL/OIT/多帧retire不属首轮承诺。
- 实例独立TRS/slot材质与PBR/Unlit已接通；textureBindings.source仅选择glTF内置纹理或禁用，不支持任意外部贴图路径加载；尚无跨场景缓存或异步上传。
- 统计使用 CommandList::drawIndexed；原始 Vulkan draw 无法自动计数。
- tests/core/test_pbr.cpp 验证CPU辅助逻辑；永久GPU fixtures/checker覆盖多材质、非平坦normal/UV、sky/env和线性Blend，阈值与证据见 [回归说明](../usage/regression-checks.md)，不是全场景历史像素基线。

## CornellBox 公共资源消费者

2026-10-21 [CornellBoxPass](../../examples/cornell_box/CornellBoxPass.cpp) 直接将程序化 MeshData 上传至 GpuModelAsset，为八个无贴图 PBR/Opaque 材质建立既有 fallback/descriptor，并用代码构造 1×1 白环境（fill=.25）。Source 长期拥有 model/material/environment/Forward与Geometry Program，四 phase Renderer 各自持有 UBO；释放 renderer 后才释放借用资源和 Program。两盒材质编辑仅改变 scalar/draw 数据，不上传几何纹理。顶灯 emissive 固定、灯强仅影响四暖点光；共享 Shader 和公共 GPU 接口未改，现有简化环境贡献不等于完整 IBL、GI、阴影或真实面光积分。详见 [04](04-triangle-example-tech.md)。
