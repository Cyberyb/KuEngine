# ForwardReuse 示例

核对日期：2026-10-07。`ForwardReuseApp` 是公共 Forward 与 Graph-owned SceneColor/Depth/display 的小型语义回归示例，不依赖外部 HDR。

从 preset 输出目录启动：

```powershell
cmake --build --preset debug --target ForwardReuseApp
Push-Location build/cmake-tools/vs2022-vcpkg/bin/Debug
try { ./ForwardReuseApp.exe } finally { Pop-Location }
```

自动化有限帧可加入 `--smoke-frames 8 --smoke-require-validation`；追加 `--smoke-recompile-after-setup` 会在 `deviceWaitIdle` 后对同一 Pipeline 第二次 compile，检查 target/descriptor 的释放与重建。`--forward-reuse-zero-lights` 将方向光和点光强度设为零；`--forward-reuse-backface` 选择背面预设。两个参数在通用 smoke 参数之前被应用剥离。

示例 fixture 是 data-URI semantic card：一个 MeshHandle、四个 primitive、两个实例、两个 GPU 材质 variant。Forward 加 Display 后预期每提交帧 **9 draws / 45 vertices**（其中 Display 为一个全屏三角形）。右侧栏的 `View Preset` 可选 `Semantic front` 或 `Double-sided back`；可选 PBR/Unlit 实例并编辑其 position、rotation、scale、base/emissive、shading，以及方向/首个点光强度。

Forward 先写 swapchain-relative SceneColor/Depth，Display 使用 sceneFramebuffer 的 UV scale/offset nearest pass-through 采样 SceneColor 到 SwapChain，随后 Overlay LOAD。resize 应只引起 target generation/Display descriptor rebind；侧栏展开、收起和重开不应额外 rebind。当前 SceneColor 是 display-ready LDR，不能把它当作线性 HDR/tone-map 测试。

人工判断：前视图的 patterned 背景应呈现独立 MR/AO 和不对称 emissive UV；checker hole 应为 Mask；半透明 tile 应在不透明深度前正确混合。把两种光都置零后 PBR 直射响应变暗而 Unlit 仍可见；背面预设中只有 cyan double-sided triangles 可从背面看到。该检查基于受控 preset 与抓帧，不包含像素 readback 或实际鼠标 UI 自动化。
