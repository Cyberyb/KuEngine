# glTF/GLB 模型加载设计

核对日期：2026-09-16。源码：[Model](../../src/KuEngine/Asset/Model.h)、[Scene](../../src/KuEngine/Asset/Scene.h)、[GpuModelAsset](../../src/KuEngine/Render/GpuModelAsset.h)。

## 输入与输出

ModelLoader::loadFromFile(path) 按扩展名调用 tinygltf 的二进制 GLB 或 ASCII glTF 加载，失败时抛出异常。返回纯 CPU MeshData，不创建 Vulkan 资源。

| 数据 | 当前字段 |
|---|---|
| MeshVertex | position、normal、uv0、uv1、tangent |
| SubMeshData | indexStart、indexCount、materialIndex |
| TextureData | width、height、RGBA8 像素 |
| MaterialData | PBR/Unlit、Opaque/Mask/Blend/cutoff/doubleSided、base/emissive 因子，metallic/roughness/normalScale/occlusionStrength，五类贴图及各自变换 |
| MeshData | vertices、uint32 indices、materials、subMeshes、AABB，兼容用基础颜色/纹理字段 |

## 场景与几何转换

默认选 defaultScene，未指定时选第 0 个 Scene。递归遍历节点，局部矩阵优先使用 matrix，否则用 T × R × S；世界矩阵为 parentWorld × local。

节点变换在加载时烘焙到 CPU 顶点。位置乘世界矩阵，法线与切线方向在当前实现中使用逆转置矩阵并归一化；切线 w 保留。导入后的 MeshData 不保留可独立更新的节点树。

仅接受 triangles Primitive；缺 POSITION 跳过，缺 NORMAL 按三角形重建。索引支持 U8/U16/U32，统一到 uint32；无索引 Primitive 生成连续索引。每个有效 Primitive 形成 SubMesh，并按基址修正索引。

## UV 与纹理

uv0/uv1 分别优先读取 TEXCOORD_0/1；缺失时尝试另一通道及第一个 TEXCOORD_*，仍无数据则回退零。GPU 顶点布局固定两组 UV，更高通道不是完整独立保留。

模型解析基础色、Normal、Metallic-Roughness、Occlusion、Emissive 贴图及 KHR_texture_transform。图片解码并转换为 RGBA8。

Metallic-Roughness 与 Occlusion 保持独立图像，shader 分别读取其 glTF 语义；不做图像重打包。每种贴图的 texCoord 与 `KHR_texture_transform` 都保留到 per-draw 数据。`KHR_materials_unlit` 映射为 Unlit；alphaMode、alphaCutoff 与 doubleSided 同样保留。

## 与运行时连接

```text
Scene JSON 模型路径
→ AssetPath / SceneLoader → SceneData (MeshHandle / Instance)
→ ModelLoader → MeshData
→ MclarenSceneAsset 合并适配
→ GpuModelAsset → GpuMesh / ResourceUploader
→ ForwardRenderer
```

SceneLoader 对同一规范路径只加载一次 MeshData，并以稳定 MeshHandle 供多个实例引用；MclarenSceneAsset 仍可合并多个模型并修正顶点/索引/材质偏移，这与在 Scene JSON 中支持逐节点实例变换是不同能力。GpuModelAsset 拥有上传后的单个 GpuMesh；Mclaren 上传后释放其合并 CPU Mesh 和 SceneData 中的 CPU Mesh，保留展示、默认和相机所需元数据。

Mclaren 可在单帧 Fence-safe update 点替换单个 GLB/glTF：候选 state 会完整重建 SceneData/默认与 fit 信息、按 handle 的 GpuModelAsset、材质 variant、ForwardRenderer 容量、pipeline 与预期统计，全部成功后才交换。直接模型替换不继承初始 Mclaren Scene JSON 的 MaterialConfig 全局颜色因子，而是使用候选模型的单位因子；该因子随已发布模型 state 一同原子切换。可选纹理缺失/无效使用 fallback；真实上传异常则令候选失败且保留 live state。

## 当前能力边界

已接入 glTF TANGENT，Shader 优先用切线构造 TBN，缺失时使用导数重建；emissive 因子与贴图已接入。

尚无 skin、animation、morph、完整 sparse accessor、压缩纹理或资源缓存。顶点只保留 uv0/uv1；高编号 UV 不构成完整顶点属性链。没有完整 IBL、OIT 或色度正确 HDR 输出。

GPU 布局与材质绑定详见 [PBR 设计](10-pbr-rendering.md)，使用方法见 [Mclaren](../usage/mclaren-example.md)。
