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
- `resources/models/props/mclaren_765lt.glb`

构建后会自动复制到运行目录：
- `build/bin/<Config>/resources/models/props/mclaren_765lt.glb`

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
  - 调整相机距离。
  - 查看顶点/索引数量与模型加载状态。

## 5. 已知约束（v0.3 当前）

- 当前已接入 baseColor/normal/ORM；暂未接入 emissive 与更多扩展 PBR 工作流。
- normal map 当前使用简化重建路径，尚未引入完整 TBN 顶点切线链路。
- 当前深度附件不包含 stencil 使用场景。
