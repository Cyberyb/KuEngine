# glTF/glb 读取与渲染接入说明（v0.3）

## 1. 目标

本文说明 KuEngine 当前如何读取 `.glb/.gltf` 并把模型与材质（baseColor/normal/ORM）接入 Vulkan 渲染链路。

适用代码入口：
- `src/KuEngine/Asset/Model.h`
- `src/KuEngine/Asset/Model.cpp`
- `examples/mclaren/MclarenPass.cpp`

---

## 2. 加载入口

- 入口函数：`ku::asset::ModelLoader::loadFromFile(path)`
- 支持：
  - `.glb` -> `TinyGLTF::LoadBinaryFromFile`
  - `.gltf` -> `TinyGLTF::LoadASCIIFromFile`

当加载成功后，输出 `MeshData`：
- 顶点数组：`position / normal / uv0 / uv1`
- 索引数组
- 子网格数组：`subMeshes`（`indexStart/indexCount/materialIndex`）
- 材质数组：`materials`
- AABB：`boundsMin / boundsMax`
- 材质参数：`baseColorFactor / metallicFactor / roughnessFactor / normalScale / occlusionStrength`
- 材质贴图：`baseColorTexture / normalTexture / ormTexture`（RGBA8）
- 纹理变换：`baseColor/normal/orm` 的 `offset/scale/rotation/texCoord`（含 `KHR_texture_transform`）

---

## 3. 场景遍历与变换

### 3.1 节点遍历

- 默认使用 `defaultScene`（若缺失则使用第 0 个 scene）。
- 递归遍历 scene 根节点及其 children。

### 3.2 节点局部矩阵

优先级：
1. 若节点直接提供 `matrix[16]`，使用该矩阵。
2. 否则使用 `T * R * S` 组合（translation/rotation/scale）。

注意：
- glTF `matrix` 是列主序（column-major）。
- 当前实现已按列主序拷贝到 GLM，避免矩阵转置导致模型姿态/比例错误。

### 3.3 世界矩阵

- `world = parentWorld * local`
- 顶点位置使用 `world * vec4(localPos, 1)`
- 法线使用 `transpose(inverse(mat3(world))) * localNormal`

---

## 4. 网格数据提取

按 primitive（当前仅处理 triangles）提取：
- `POSITION` -> 顶点位置
- `NORMAL` -> 顶点法线（可选）
- `TEXCOORD_0 / TEXCOORD_1` -> UV（可选）
- `indices` -> 索引（支持 U8/U16/U32）

兼容策略：
- 若缺失 NORMAL：按三角面累计并归一化重建法线。
- 若缺失 `TEXCOORD_0/TEXCOORD_1`：对应 UV 默认为 `(0, 0)`。
- 每个 primitive 生成一个 submesh，记录其索引范围与 `materialIndex`。

---

## 5. 材质与贴图读取（当前范围）

当前已接入 glTF PBR 的以下链路：
- `material.pbrMetallicRoughness.baseColorFactor`
- `material.pbrMetallicRoughness.metallicFactor / roughnessFactor`
- `material.pbrMetallicRoughness.baseColorTexture`
- `material.normalTexture`（含 `scale`）
- `material.occlusionTexture`（含 `strength`）
- `material.pbrMetallicRoughness.metallicRoughnessTexture`

其中 ORM 贴图按统一路径处理：
- 若存在 `occlusionTexture`，优先使用其图像作为 ORM；
- 否则使用 `metallicRoughnessTexture`；
- 着色时约定通道：`R=occlusion, G=roughness, B=metallic`。

贴图处理流程：
1. 找到 texture -> image。
2. 读取像素并转换到 RGBA8。
3. 存入对应材质纹理字段（baseColor/normal/orm）。

说明：
- 当前按 primitive 生成 submesh，并按 `materialIndex` 分批绘制。
- 每个 submesh 使用其对应材质参数和纹理。
- 支持 `KHR_texture_transform`（含 `texCoord` 覆盖）。

### 5.1 UV 通道选择策略

为降低资产差异导致的“有贴图但采样不到”问题，当前实现对每类贴图分别按以下顺序选择 UV：

1. 优先使用材质 `baseColorTexture.texCoord` 指定的 `TEXCOORD_n`。
2. 若指定通道不存在，回退到 `TEXCOORD_0`。
3. 若仍不存在，尝试 `TEXCOORD_1`。
4. 若仍不存在，则在 primitive 属性里查找首个 `TEXCOORD_*`。

当所有 UV 通道都不存在时，UV 默认为 `(0, 0)`，此时纹理会退化为单点采样效果。

### 5.2 纹理变换

若材质使用 `KHR_texture_transform`，会在 shader 中对 UV 执行：
- 先按 `scale` 缩放；
- 再按 `rotation` 旋转；
- 最后加 `offset`。

此外保留运行时 `Flip UV-Y` 开关，用于快速验证资产坐标朝向差异。

---

## 6. Vulkan 侧上传与绑定

在 `MclarenPass` 中执行：

### 6.1 几何数据

- 通过 `RHIBuffer` 创建顶点缓冲与索引缓冲。
- Host visible staging 写入后供绘制使用。

### 6.2 纹理数据

- 创建 `RHITexture`（`VK_FORMAT_R8G8B8A8_SRGB`）。
- 使用 `CommandList::copyBufferToImage` 上传。
- 布局转换：
  - `UNDEFINED -> TRANSFER_DST_OPTIMAL`
  - `TRANSFER_DST_OPTIMAL -> SHADER_READ_ONLY_OPTIMAL`

### 6.3 描述符与采样器

- 创建 `descriptor set layout`：
  - `set=0, binding=0` -> baseColor
  - `set=0, binding=1` -> normal
  - `set=0, binding=2` -> orm
- 创建 sampler（线性过滤，支持时启用各向异性）
- 为每个材质分配 descriptor set，并写入对应纹理
- 无纹理时使用回退纹理：
  - baseColor -> 1x1 白色
  - normal -> 1x1 平面法线 `(128,128,255)`
  - orm -> 1x1 默认 `(255,255,255)`

### 6.4 着色器使用

- 顶点着色器输出 `uv0/uv1`、世界空间法线与位置。
- 片元着色器按材质配置采样 baseColor/normal/orm。
- 法线贴图启用时在 shader 中进行重建与混合。
- ORM 与材质因子共同参与光照计算。
- 提供开关：`Use BaseColor Texture / Use Normal Map / Use ORM Map / Flip UV-Y`。

---

## 7. 深度附件接入

在 `examples/mclaren/main.cpp` 中：

1. 选择支持的深度格式（优先 `D32_SFLOAT`）。
2. 创建深度 `RHITexture`（usage: `DEPTH_STENCIL_ATTACHMENT_BIT`）。
3. 初始布局转换到 `DEPTH_ATTACHMENT_OPTIMAL`。
4. 在 dynamic rendering 的 `VkRenderingInfo` 中绑定 `pDepthAttachment`。
5. swapchain resize/recreate 时同步重建深度附件。

管线侧：
- `depthTest = true`
- `depthWrite = true`
- `depthFormat = 选定深度格式`

---

## 8. 当前限制

- 暂未接入 emissive、clearcoat、transmission、specular 等扩展 PBR 工作流。
- normal map 当前在示例中采用简化重建路径，尚未引入完整 TBN 顶点属性链路。
- 暂未处理 glTF sparse accessor、skin、morph target、animation。

---

## 9. 后续建议（v0.3+）

1. 完整接入切线空间（TANGENT + TBN）以提升 normal map 准确性。
2. 扩展 emissive/alphaMode/alphaCutoff 与双面材质策略。
3. 增加纹理 color space 与通道校验（特别是 normal/ORM 的线性空间约束）。
4. 增加资产导入诊断日志（缺失语义、非法 accessor、纹理格式不支持等）。
