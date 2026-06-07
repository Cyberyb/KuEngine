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

运行模式说明：
- 前台模式（默认）：阻塞等待应用退出，并返回真实退出码（适合调试）。
- 后台模式（可选）：异步拉起窗口并立即返回（适合快速启动）。

常用命令：
- 前台（默认 Debug）：`examples/mclaren/run_mclaren.bat`
- 前台（指定配置）：`examples/mclaren/run_mclaren.bat Debug`
- 后台（默认 Debug）：`examples/mclaren/run_mclaren.bat bg`
- 后台（指定配置）：`examples/mclaren/run_mclaren.bat Debug bg`

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

- 当前已接入 baseColor/normal/ORM；emissive 仅支持因子，未接入 emissive 贴图。
- normal map 现优先使用 glTF TANGENT 生成 TBN；缺失时回退到屏幕空间导数重建。
- 当前深度附件不包含 stencil 使用场景。
- 多节点场景目前合并为单批次渲染，尚未支持 per-node 变换与材质覆盖。

## 6. v0.3 进展状态（2026-05-13）

已完成：
- Scene/Material JSON 已接入示例读取（缺失时自动回退默认参数）。
- 相机参数（FOV/near/far）与基础光照参数（direction/color/intensity）已真实驱动 shader。
- 新增资产配置解析模块：`src/KuEngine/Asset/AssetConfig.{h,cpp}`。
- 新增回归测试：`tests/core/test_asset_config.cpp`，覆盖解析成功、缺字段默认值、文件缺失报错路径。
- Material JSON 现可解析 alpha/metallic/roughness/normalScale/occlusionStrength 与 textureBindings 字段。
- Material JSON 的 textureBindings 现可驱动 glTF 纹理选择、UV 通道与色彩空间格式。
- Scene JSON 支持多节点模型合并加载（多个模型会合并为同一渲染批次）。
- glTF emissiveFactor 已接入渲染输出，alphaMode=MASK/BLEND 可在材质 JSON 中生效。
- normal map 支持 glTF TANGENT 路径的 TBN 法线重建。
- textureBindings 会校验 colorSpace，并在发现不合理设置时回退到推荐格式。

未完成：
- 材质 JSON 目前仅驱动 `baseColorFactor`，其他材质参数仍未接入渲染路径。
- textureBindings 尚不支持外部贴图路径（仅支持 glTF 内置纹理选择）。
- 多节点场景暂不支持 per-node 变换与 per-node 材质配置（仅使用首个 material.json）。
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

## 8. Shader 源码调试编译模式（RenderDoc）

为便于在 RenderDoc 中按 GLSL 源码行定位问题，Mclaren 示例支持“源码调试编译模式”。

快速开启（PowerShell）：

```powershell
$env:KU_SHADER_SOURCE_DEBUG = "1"
./examples/mclaren/run_mclaren.bat
```

快速关闭（恢复默认）：

```powershell
Remove-Item Env:KU_SHADER_SOURCE_DEBUG -ErrorAction SilentlyContinue
./examples/mclaren/run_mclaren.bat
```

运行日志中可通过以下字段确认模式：
- `Shader compile mode: source-debug-gVS`
- `Shader source debug compiler: glslangValidator -gVS -Od`

运行时可通过 UI 开关快速对比颜色空间效果：
- `Encode Output Gamma (Linear->sRGB)`：开启时执行输出 gamma 编码；关闭时输出线性颜色。

详细机制、优先级与排障请参考：
- `docs/design/09-shader-source-debug-mode.md`
