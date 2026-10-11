# glTF/GLB 模型加载设计

核对日期：2026-10-11（C核对既有模型替换边界）。源码：[Model](../../src/KuEngine/Asset/Model.h)、[Scene](../../src/KuEngine/Asset/Scene.h)、[GpuModelAsset](../../src/KuEngine/Render/GpuModelAsset.h)。

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
→ MclarenSceneAsset 导入fit / 轻量实例与slot元数据
→ GpuModelAsset → GpuMesh / ResourceUploader
→ ForwardRenderer
```

SceneLoader对同一规范路径只加载一次MeshData，稳定MeshHandle供多个SceneInstance引用；glTF内部节点已烘焙，不是查看器可编辑实例。GpuModelAsset拥有单个GpuMesh，Mclaren按handle上传一次。`releaseCpuMesh()`以空vector swap释放vertices/indices/所有纹理pixels(capacity=0)，HDR像素也释放；保留mesh bounds/subMeshes/materials(含纹理元信息)、实例TRS/materialSlots与路径。编辑不调用ModelLoader或重传GPU mesh/texture/descriptor。SceneData.valid仍严格完整导入校验，不因释放而放宽；导入fit静态缓存不随编辑重frame相机。

Mclaren 可在单帧 Fence-safe update 点替换单个 GLB/glTF：候选 state 会完整重建 SceneData/默认与 fit 信息、按 handle 的 GpuModelAsset、材质 variant、ForwardRenderer 容量、pipeline 与预期统计，全部成功后才交换。直接模型替换不继承初始 Mclaren Scene JSON 的 MaterialConfig 全局颜色因子，而是使用候选模型的单位因子；该因子随已发布模型 state 一同原子切换。可选纹理缺失/无效使用 fallback；真实上传异常则令候选失败且保留 live state。

## 当前能力边界

侧栏成功Model Load由`MclarenSceneAsset::loadModel`构建单节点`replacement-model`新SceneData，发布后instance/slot选择重置0，不保留初始双实例编辑。普通path/HDR/resize/recompile保留场景；失败模型候选保留当前新模型。这是既有实现，最终C独立SessionB验证success request5/failed request6；不能声称A3双实例generation19跨进程保留。

已接入 glTF TANGENT，Shader 优先用切线构造 TBN，缺失时使用导数重建；emissive 因子与贴图已接入。

尚无 skin、animation、morph、完整 sparse accessor、压缩纹理或资源缓存。顶点只保留uv0/uv1，高编号UV不是完整属性链。WP02未修改loader接口/所有权；新增非平坦normal fixture经现有glTF链路验证UV1/texture-transform/normalScale。统一线性HDR/显示事实见 [Forward](11-forward-rendering.md)，完整IBL/OIT不是模型loader职责或本次首轮范围。

GPU 布局与材质绑定详见 [PBR 设计](10-pbr-rendering.md)，使用方法见 [Mclaren](../usage/mclaren-example.md)。
