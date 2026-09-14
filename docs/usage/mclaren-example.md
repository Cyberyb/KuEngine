# Mclaren 示例使用说明

核对日期：2026-09-14。示例使用公共 Engine Runtime，加载 GLB/JSON/HDR，在一个 Pass 中绘制 Skybox 与 PBR 模型。

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

- Camera 区域的 `Mode` 选择 `Orbit Inspect` 或 `Free Fly`。两种模式切换时从当前相机帧交接位置/朝向，切换后先松开所有相关按键/鼠标，再开始新的操作，避免把切换前的 held 输入带入新模式。
- Orbit Inspect：左键必须从场景区域开始拖动，旋转模型；场景区域滚轮缩放。`Reset View` 恢复配置视图，`Reset Model Rotation` 只清模型旋转。
- Free Fly：先在场景区域按下右键取得焦点；保持焦点时 `W/S/A/D` 前后左右、`Q/E` 下上，右键拖动转向，`Move Speed` 调整速度。点击侧栏、编辑文字、UI 捕获键盘/鼠标、窗口失焦或恢复都会取消焦点；重新操作前需要先释放输入。`Reset View` 在该模式下恢复自由相机视图，`Reset Model Rotation` 仍只影响 Orbit 模型旋转。
- 侧栏或被 UI 捕获区域不驱动相机。投影比例与交换链场景 viewport 共享同一布局，不再存在第二套 Mclaren 局部视口。
- 控制 BaseColor、Normal、ORM 采样，UV-Y 翻转与输出 Gamma。
- 调整相机投影、可见视口、光照和全局颜色因子。
- 开关 Skybox、环境反射并调整环境强度/曝光。
- 查看模型 Vertex/Index/SubMesh 数量和加载错误。
- 右侧唯一侧栏提供 Performance、Parameters / Scene、Render Graph；可收起为紧凑统计或仅重开控件。公共统计显示 FPS、Frame、CPU/GPU 时间与业务 draw/顶点提交数。

公共 Vertices 是提交索引/顶点数量，模型面板 Vertices 是 GPU Mesh 顶点容量；两者通常不同。CPU/GPU/Draw/Vertices 显示同一 completed submit，FPS/Frame 是当前主循环样本，详细口径见 [UI 设计](../design/05-ui-layer.md)。启用 Skybox 时预期计数为一个 3 顶点 draw 加每个非空 SubMesh 的 indexed draw；关闭 Skybox 少一个 draw 和 3 个提交顶点。

## 当前限制

emissive 因子和贴图、TANGENT/TBN 已接入。多模型合并仍不支持 JSON 的逐节点变换或独立材质覆盖；多个材质 JSON 仅使用首个。外部贴图路径、完整 IBL、透明排序和独立 emissive UV 链路尚不完整。

Free Fly 目前只属于 Mclaren 示例层，尚未抽为公共场景/相机服务，也未接入 Cube。QA 已在 Mclaren 实机确认两向模式切换无明显跳变、文本输入不移动、reset、侧栏收展和最大化/最小化；持续右键拖动及 WASDQE 六向、组合 held 手势没有可靠实机手势记录，不能据此声称已经人工覆盖。Win+D、物理非等比 DPI 和像素参考比对同样未覆盖。

Runtime 持有深度；不要在示例中重复创建交换链深度资源。完整设计见 [示例设计](../design/04-triangle-example-tech.md) 和 [PBR](../design/10-pbr-rendering.md)。

运行检查见 [回归指南](regression-checks.md)，源码调试见 [Shader 调试](shader-debugging.md)。
