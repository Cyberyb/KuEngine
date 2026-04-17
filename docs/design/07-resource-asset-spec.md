# v0.3 资产规范（Resources）

## 1. 文档目标

本规范用于统一 `resources/` 目录下的资产组织方式，解决 v0.3 阶段模型、纹理、材质和场景资源的落盘混乱问题。

适用范围：
- v0.3 及后续版本的示例资源、测试资源、算法验证资源。
- 运行时可直接加载的资产（runtime）与 DCC 原始资产（source）。

---

## 2. 顶层目录规范

`resources/` 必须使用以下结构：

```text
resources/
├── manifests/                      # 资产清单与索引
├── models/                         # 模型资产（按类别）
│   ├── characters/
│   ├── props/
│   ├── environments/
│   └── kits/
├── textures/                       # 纹理资产（按用途）
│   ├── pbr/
│   │   ├── basecolor/
│   │   ├── normal/
│   │   ├── orm/
│   │   ├── emissive/
│   │   ├── height/
│   │   └── opacity/
│   ├── ui/
│   ├── lookup/
│   └── noise/
├── materials/                      # 材质定义（JSON）
│   ├── pbr/
│   └── unlit/
├── scenes/                         # 场景描述（最小 glTF 场景或自定义场景 JSON）
│   ├── sandbox/
│   └── regression/
├── environments/                   # 环境贴图
│   ├── hdr/
│   └── skybox/
└── source/                         # DCC 原始文件，不直接给运行时读取
    ├── dcc/
    └── scans/
```

---

## 3. 分类与存放规则

| 类型 | 存放路径 | 推荐格式 | 说明 |
|------|----------|----------|------|
| 模型（运行时） | `resources/models/<category>/<asset-id>/runtime/` | `.glb`（优先）或 `.gltf + .bin` | v0.3 统一以 glTF 2.0 作为运行时模型输入 |
| 模型（原始） | `resources/source/dcc/<asset-id>/` | `.blend` `.fbx` `.ma` 等 | 原始文件只存档，不直接参与运行 |
| 纹理（PBR） | `resources/textures/pbr/<map-type>/` | `.png` `.jpg` `.ktx2` | map-type 见下方命名规范 |
| 材质 | `resources/materials/<pipeline>/` | `.material.json` | 存纹理绑定、参数、宏开关 |
| 场景 | `resources/scenes/<scene-name>/` | `.scene.json` 或 `.gltf/.glb` | 最小可运行场景描述 |
| 环境贴图 | `resources/environments/<kind>/` | `.hdr` `.ktx2` `.dds` | `hdr` 为 equirect，`skybox` 为立方体贴图 |
| 资产清单 | `resources/manifests/` | `.json` | 记录 ID、版本、来源与许可证 |

---

## 4. 命名规范

### 4.1 通用命名

- 文件名与目录名统一使用：`lower-kebab-case`。
- 资产主 ID 建议：`<domain>-<name>-<variant>`（variant 可选）。
- 禁止中文、空格和大写字母。

示例：
- `crate-wood`
- `robot-biped-v2`
- `city-block-a`

### 4.2 纹理后缀规范

纹理命名格式：`<asset-id>_<suffix>.<ext>`。

| suffix | 含义 | 色彩空间 |
|--------|------|----------|
| `basecolor` | 基础色 | sRGB |
| `normal` | 法线 | Linear |
| `orm` | AO/Roughness/Metallic 打包图（R/G/B） | Linear |
| `emissive` | 自发光 | sRGB |
| `height` | 高度/位移 | Linear |
| `opacity` | 透明度 | Linear |

示例：
- `crate-wood_basecolor.png`
- `crate-wood_normal.png`
- `crate-wood_orm.png`

### 4.3 LOD/变体命名

- LOD 模型：`<asset-id>_lod0.glb`、`<asset-id>_lod1.glb`。
- 贴图变体：`<asset-id>_<suffix>_v02.png`。

---

## 5. 模型资产包规范

一个模型资产建议使用独立目录：

```text
resources/models/props/crate-wood/
├── runtime/
│   ├── crate-wood.glb
│   └── textures/                      # 仅当模型依赖相对路径贴图时使用
├── meta/
│   └── crate-wood.asset.json
└── README.md
```

`meta/<asset-id>.asset.json` 建议字段：

```json
{
  "id": "crate-wood",
  "type": "model",
  "category": "props",
  "version": "1.0.0",
  "source": "internal|external",
  "license": "CC0|Custom",
  "unit": "meter",
  "upAxis": "Y",
  "tags": ["wood", "container"]
}
```

---

## 6. 导入流程（v0.3）

1. 原始资产先放入 `resources/source/`。
2. 导出运行时资产到 `resources/models/` 与 `resources/textures/`。
3. 在 `resources/materials/` 写材质定义（贴图绑定和参数）。
4. 在 `resources/manifests/asset-registry.template.json` 中登记资产条目。
5. 本地运行示例，验证：路径可加载、坐标系正确、贴图通道正确。

---

## 7. 禁止项

- 禁止把模型、纹理直接散放在 `resources/` 根目录。
- 禁止把 DCC 原始文件和运行时文件混放在同一目录。
- 禁止同一资产 ID 出现多套互相覆盖的命名规则。
- 禁止未标注来源和许可证的外部资产入库。

---

## 8. v0.3 最小验收清单（资产侧）

- 至少 1 个 glTF 模型包按本规范落盘。
- 至少 1 套完整 PBR 纹理（basecolor/normal/orm）按规范命名。
- 至少 1 个材质 JSON 能完成纹理绑定。
- 至少 1 个最小场景可加载并用于算法验证。

---

## 9. 后续扩展（v0.4+）

- 增加动画资源目录（`animations/`）及骨骼命名规范。
- 增加压缩纹理基线（KTX2 + BasisU）与自动转码流程。
- 增加资产校验脚本（命名、尺寸、通道、许可证）并接入 CI。
