# CornellBox 程序化延迟 PBR 示例

核对日期：2026-10-21。`CornellBoxApp` 自行生成开口房间、红左墙/绿右墙、白色地顶背墙、两个旋转白盒与可见顶灯，不需要下载模型、JSON 或外部 HDR。默认使用公共 Deferred/PBR；设计见 [示例设计](../design/04-triangle-example-tech.md)。

## 构建与启动

在仓库根目录运行 bat（默认 Debug；也可显式 Release）：

```powershell
.\examples\cornell_box\run_cornell_box.bat
.\examples\cornell_box\run_cornell_box.bat Release
```

bat 配置/构建 `build`，进入 `build/bin/Debug` 或 `build/bin/Release` 前台运行并返回应用退出码；公共 Shader target 随构建执行，无需另找 shader 编译脚本。bat 本轮只做源码核对，独立 QA 使用等价 CMake/应用命令实际构建与启动。

使用 preset 时输出在另一构建目录；从仓库根配置并构建：

```powershell
cmake --preset vs2022-vcpkg
cmake --build --preset debug --target CornellBoxApp
Push-Location build/cmake-tools/vs2022-vcpkg/bin/Debug
.\CornellBoxApp.exe
Pop-Location
```

Release 对应 `cmake --build --preset release --target CornellBoxApp`，启动目录为 `build/cmake-tools/vs2022-vcpkg/bin/Release`。手动启动始终进入对应 exe 目录，保证相对公共 Shader 路径正确；不要混用 preset、bat 和独立 QA 的构建目录。

## 参数与控件

默认窗口 1280×960，Deferred；从 exe 目录启动的常用参数：

| 参数 | 用途 |
|---|---|
| `--render-path forward` / `--render-path deferred` | 显式选择路径；`--deferred` 是公共兼容入口 |
| `--smoke-frames N` | 按实际成功提交 N 帧后退出；0 为持续交互 |
| `--smoke-resize-after FRAME WIDTH HEIGHT` | 指定提交帧后请求 resize，帧上限至少多两帧 |
| `--smoke-recompile-after-setup` | setup 后在 idle 边界再 compile |
| `--smoke-require-validation` | 要求可捕获 Validation；环境不可用返回77，不能视作 GPU 通过 |
| `--smoke-sidebar-collapsed` | 从收起侧栏启动 |
| `--cornell-readback-prefix <path>` | 普通 Deferred 有限帧证据导出，见下节 |

Rendering 区可切换 Forward/Deferred 与公共八种 GBuffer 视图；Forward 禁用 GBuffer，切至 Forward 恢复 Scene。Scene 的控件如下：

| 控件 | 范围 / 含义 |
|---|---|
| Ceiling light | 0～3，缩放四个暖色点光；灯面固定 emissive 保持可见 |
| Exposure / Neutral fill | .1～3 / 0～.5；默认1 / .25 |
| Material | 选择 Short box 或 Tall box，独立编辑 Roughness (.04～1) / Metallic (0～1) |
| Camera yaw / pitch / distance | -16～16° / -12～12° / 3.2～5.5 |
| Reset view | 恢复 yaw=0、pitch=0、distance=4.1 |

路径、resize/recompile 和侧栏收展保留参数。相机通过上述 UI 参数控制，没有新增鼠标 Orbit 或 FreeFly。普通 Deferred 统计为10 draws/114 submitted vertices，Forward 为9/111；默认 skybox 关闭，Background 为0/0；Deferred为8个几何draw加Lighting/Display两个全屏三角形，Forward为8个几何draw加Display。这是提交索引数，不是72个去重几何顶点，也不包含 ImGui draw。

照明沿用公共简化 Metallic-Roughness PBR：四点光负责直接照明，1×1 中性环境补光，顶灯 emissive=(12,10.8,9)。当前没有阴影、GI、真实面积光积分或完整 IBL，不是路径追踪 Cornell 参考图。

## 有限帧 GPU 证据

下面从任一已构建的 `bin/Release` 目录运行；输出路径可自行指定，路径带空格时需加引号。相同命令也可用于 Debug，并通过日志确认实际启用 Validation。

```powershell
.\CornellBoxApp.exe --smoke-frames 4 --cornell-readback-prefix cornell-evidence/default
.\CornellBoxApp.exe --smoke-frames 6 --smoke-resize-after 2 1120 800 --smoke-recompile-after-setup --cornell-readback-prefix cornell-evidence/resize
.\CornellBoxApp.exe --render-path forward --smoke-frames 3
```

独立 QA 的构建位于 `build/qa-cornell-box`，真实应用工作目录分别为其 `bin/Debug`、`bin/Release`。Debug GPU 测试注册为 `gpu_smoke_cornell_box`、`gpu_smoke_cornell_box_resize_recompile`；在仓库根可定向运行：

```powershell
ctest --test-dir build/qa-cornell-box -C Debug --output-on-failure -R '^gpu_smoke_cornell_box(_resize_recompile)?$'
```

注册这两项需要配置 `KUENGINE_ENABLE_GPU_SMOKE_TESTS=ON`。Debug 显式同步验证可在启动前设置 `VK_LAYER_ENABLES=VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT`，结束后恢复原环境值。Release 的真实 GPU 成功不替代 Debug Validation 证据。

读回输出 `<prefix>_<width>x<height>.rawbin` 与 `.ppm`。rawbin 先存两个 uint32（width/height），随后按 BaseMetallic、PositionModel、线性 HDR 顺序存三张全附件 RGBA16F 原始 half 平面，每像素每图8字节。PPM 仅截取场景 viewport，将实际 HDR 以当前曝光、Reinhard/sRGB 在 CPU 转换为预览；不包含 UI，不是 Swapchain 截图。

staging 容量为2048²像素，Graph声明传输与HOST_READ边界，单帧Fence完成后读取。此功能只用于普通Deferred/PBR的有限帧场景检查；启动Forward并请求读回返回参数错误，不将任意交互材质、debug或路径切换时的读回视作通用截图能力。正常输出含 `KUENGINE_CORNELL_PIXELS_OK`、`KUENGINE_COMPLETED_STATS ... status=matched` 和 smoke pass，marker仍需结合原始像素/画面检查。

## 本包验证结果

2026-10-21 [独立 QA 正式 ACCEPTED](../../build/qa-cornell-box/qa-report.md)：Debug/Release 独立配置构建通过，各core142/142；Debug定向CTest7/7且0skip，包含相机组合40、Cornell两项、Triangle和ForwardReuse相关回归，显式SyncVal errors=0。三组真实Release默认4帧/resize-recompile6帧/Forward3帧均退出0、统计matched。

独立解析GPU half数据并以ray/plane/OBB检查默认2499、resize1720个最近可见表面样本，世界位置最大误差<.001，红左绿右、PBRcode1、灯面HDR>12和两盒空间/遮挡通过；真实GUI观察灯强、曝光、两盒独立M/R、Base/Normal/Roughness/Metallic/ShadingModel/Scene、路径保留、相机与侧栏通过，正常退出0。真实HDR转换的[默认预览](../../build/qa-cornell-box/final-default.png)仅作场景预览。

本轮未运行完整33项CTest/全部旧M4 GPU、RenderDoc、低能力设备、soak、额外FreeFly/minimize/focus stress或bat实跑。完整证据与NOT RUN见 [回归说明](regression-checks.md)，不改变历史M4验收记录。
