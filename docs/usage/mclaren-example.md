# Mclaren 示例使用说明

核对日期：2026-09-12。示例使用公共 Engine Runtime，加载 GLB/JSON/HDR，在一个 Pass 中绘制 Skybox 与 PBR 模型。

## 构建和启动

一键运行，在仓库根目录执行：

```powershell
./examples/mclaren/run_mclaren.bat Debug
```

脚本使用 build/bin/Debug，默认前台等待应用退出。可选 `Debug bg` 异步启动；后台脚本退出不表示应用已成功完成渲染。

使用 preset 的完整路径为：

```powershell
cmake --preset vs2022-vcpkg
cmake --build --preset debug --target MclarenApp
Push-Location build/cmake-tools/vs2022-vcpkg/bin/Debug
try { ./MclarenApp.exe } finally { Pop-Location }
```

## 输入资源

默认场景是 resources/scenes/sandbox/mclaren-sandbox.scene.json，引用模型 models/props/mclaren_765lt.glb 和材质 materials/pbr/mclaren-765lt.material.json。环境为 resources/environments/hdr/citrus_orchard_road_puresky_4k.hdr。

CMake 将已有资源复制到 exe 旁，加载优先使用运行目录副本，再回退源码目录。修改源码 JSON 后，需要更新运行目录副本才会生效。asset-registry.json 会被复制，但示例没有通过 Registry 自动发现资源。

## 交互与面板

- 左键拖动旋转，视口内滚轮缩放。
- 控制 BaseColor、Normal、ORM 采样，UV-Y 翻转与输出 Gamma。
- 调整相机投影、可见视口、光照和全局颜色因子。
- 开关 Skybox、环境反射并调整环境强度/曝光。
- 查看模型 Vertex/Index/SubMesh 数量和加载错误。
- 公共统计面板显示 FPS、Frame、CPU/GPU 时间与业务 draw/顶点提交数。

公共 Vertices 是提交索引/顶点数量，模型面板 Vertices 是 GPU Mesh 顶点容量；两者通常不同。CPU/GPU 显示最近完成帧，详细口径见 [UI 设计](../design/05-ui-layer.md)。

## 当前限制

emissive 因子和贴图、TANGENT/TBN 已接入。多模型合并仍不支持 JSON 的逐节点变换或独立材质覆盖；多个材质 JSON 仅使用首个。外部贴图路径、完整 IBL、透明排序和独立 emissive UV 链路尚不完整。

Runtime 持有深度；不要在示例中重复创建交换链深度资源。完整设计见 [示例设计](../design/04-triangle-example-tech.md) 和 [PBR](../design/10-pbr-rendering.md)。

运行检查见 [回归指南](regression-checks.md)，源码调试见 [Shader 调试](shader-debugging.md)。
