# Mclaren 示例使用说明

## 1. 目标

`MclarenApp` 用于验证 KuEngine v0.3 的模型导入与材质渲染链路：
- 从 `.glb` 载入网格。
- 读取 glTF PBR 材质中的 baseColor/normal/ORM 与关键因子。
- 按 primitive/submesh 的 `materialIndex` 分材质绘制。
- 支持 `KHR_texture_transform`（offset/scale/rotation/texCoord）。
- 通过顶点/索引缓冲渲染模型。
- 使用动态渲染深度附件进行深度测试与写入。
- 鼠标左键拖拽旋转模型。

## 2. 资源路径

示例默认读取：
- `resources/scenes/sandbox/mclaren-sandbox.scene.json`
- `resources/materials/pbr/mclaren-765lt.material.json`
- `resources/models/props/mclaren_765lt.glb`
- `resources/manifests/asset-registry.json`

构建后会自动复制到运行目录：
- `build/bin/<Config>/resources/scenes/sandbox/mclaren-sandbox.scene.json`
- `build/bin/<Config>/resources/materials/pbr/mclaren-765lt.material.json`
- `build/bin/<Config>/resources/models/props/mclaren_765lt.glb`
- `build/bin/<Config>/resources/manifests/asset-registry.json`

## 3. 构建运行

方式 A（推荐，一键脚本）：
- `examples/mclaren/run_mclaren.bat`

方式 B（手动）：
1. `cmake --preset vs2022-vcpkg`
2. `cmake --build --preset debug --target MclarenApp`
3. 运行 `build/bin/Debug/MclarenApp.exe`

## 4. 交互

- 左键按住拖拽：旋转模型。
- UI 面板：
  - 开关 BaseColor/Normal/ORM 采样。
  - 调整全局 baseColorFactor。
  - 开关 `Flip UV-Y` 以快速排查 UV 朝向差异。
  - 调整相机参数（距离、FOV、near/far）。
  - 调整基础光照参数（direction/color/intensity）。
  - 查看顶点/索引数量与模型加载状态。

## 5. 已知约束（v0.3 当前）

- 当前已接入 baseColor/normal/ORM；暂未接入 emissive 与更多扩展 PBR 工作流。
- normal map 当前使用简化重建路径，尚未引入完整 TBN 顶点切线链路。
- 当前深度附件不包含 stencil 使用场景。
- 场景节点当前按最小实现读取首个 `nodes[0]` 作为示例输入。

## 6. v0.3 进展状态（2026-04-19）

已完成：
- Scene/Material JSON 已接入示例读取（缺失时自动回退默认参数）。
- 相机参数（FOV/near/far）与基础光照参数（direction/color/intensity）已真实驱动 shader。
- 新增资产配置解析模块：`src/KuEngine/Asset/AssetConfig.{h,cpp}`。
- 新增回归测试：`tests/core/test_asset_config.cpp`，覆盖解析成功、缺字段默认值、文件缺失报错路径。

未完成：
- 材质 JSON 目前仅驱动 `baseColorFactor`，纹理绑定的完整接管仍以 glTF 内材质为主。
- 多节点场景装配（遍历所有 nodes 并批量实例化）尚未接入。
- emissive/clearcoat/transmission 等扩展 PBR 尚未接入。

## 7. 最小回归命令（v0.3 当前）

在仓库根目录执行：

```powershell
cmake --build --preset debug --target MclarenApp
ctest --preset debug --output-on-failure
./examples/mclaren/run_mclaren.bat
```

期望：
- `MclarenApp` 构建成功。
- `core_tests` 通过。
- 示例可启动，且 UI 中相机/光照参数调整可见生效。
