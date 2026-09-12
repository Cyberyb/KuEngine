# glTF/GLB 模型加载设计

核对日期：2026-09-12。源码：[Model.h](../../src/KuEngine/Asset/Model.h)、[Model.cpp](../../src/KuEngine/Asset/Model.cpp)。

## 输入与输出

ModelLoader::loadFromFile(path) 按扩展名调用 tinygltf 的二进制 GLB 或 ASCII glTF 加载，失败时抛出异常。返回纯 CPU MeshData，不创建 Vulkan 资源。

| 数据 | 当前字段 |
|---|---|
| MeshVertex | position、normal、uv0、uv1、tangent |
| SubMeshData | indexStart、indexCount、materialIndex |
| TextureData | width、height、RGBA8 像素 |
| MaterialData | baseColor/emissive 因子，metallic/roughness/normalScale/occlusionStrength，四类贴图与变换 |
| MeshData | vertices、uint32 indices、materials、subMeshes、AABB，兼容用基础颜色/纹理字段 |

## 场景与几何转换

默认选 defaultScene，未指定时选第 0 个 Scene。递归遍历节点，局部矩阵优先使用 matrix，否则用 T × R × S；世界矩阵为 parentWorld × local。

节点变换在加载时烘焙到 CPU 顶点。位置乘世界矩阵，法线与切线方向在当前实现中使用逆转置矩阵并归一化；切线 w 保留。导入后的 MeshData 不保留可独立更新的节点树。

仅接受 triangles Primitive；缺 POSITION 跳过，缺 NORMAL 按三角形重建。索引支持 U8/U16/U32，统一到 uint32；无索引 Primitive 生成连续索引。每个有效 Primitive 形成 SubMesh，并按基址修正索引。

## UV 与纹理

uv0/uv1 分别优先读取 TEXCOORD_0/1；缺失时尝试另一通道及第一个 TEXCOORD_*，仍无数据则回退零。GPU 顶点布局固定两组 UV，更高通道不是完整独立保留。

模型解析基础色、Normal、Metallic-Roughness、Occlusion、Emissive 贴图及 KHR_texture_transform。图片解码并转换为 RGBA8。

ORM 当前优先取 Metallic-Roughness 图像；缺失才回退 Occlusion 图像。Shader 按 R=AO、G=roughness、B=metallic 使用，未把独立 AO 和 MR 图像重新打包，因此不能宣称完整兼容所有 glTF 的独立通道布局。

## 与运行时连接

```text
Scene JSON 模型路径
→ MclarenSceneAsset
→ ModelLoader → MeshData
→ MclarenRenderResources
→ GpuMesh / TextureFactory → ResourceUploader
→ PBRRenderer
```

MclarenSceneAsset 可合并多个模型，修正顶点/索引/材质偏移；这与在 Scene JSON 中支持逐节点实例化是不同的能力。GPU 上传后释放 CPU Mesh，保留展示和相机需要的场景元数据。

## 当前能力边界

已接入 glTF TANGENT，Shader 优先用切线构造 TBN，缺失时使用导数重建；emissive 因子与贴图已接入。

尚无 skin、animation、morph、完整 sparse accessor、压缩纹理或资源缓存。MaterialData 也没有完整保留 glTF alphaMode/doubleSided；当前 Mclaren 的 alpha 策略主要来自 Material JSON。emissive 变换虽在 CPU 中解析，Shader 当前使用 uvBase 采样，不能视为独立 emissive UV 全链路完成。

GPU 布局与材质绑定详见 [PBR 设计](10-pbr-rendering.md)，使用方法见 [Mclaren](../usage/mclaren-example.md)。
