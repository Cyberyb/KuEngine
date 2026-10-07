# GraphResourceProbe 示例

核对日期：2026-10-07。`GraphResourceProbeApp` 是 M3-WP01～04 的图内资源、同步与 callback/compute 示例。它顺序注册六个 Pass：ImageClear、BufferFill、native Copy callback、normal Compute callback、SampleDraw 与 PostDrawCopy；统计为 1 draw/3 vertices。

从 preset 输出目录启动：

```powershell
cmake --build --preset debug --target GraphResourceProbeApp
Push-Location build/cmake-tools/vs2022-vcpkg/bin/Debug
try { ./GraphResourceProbeApp.exe --smoke-frames 8 --smoke-require-validation --smoke-recompile-after-setup --smoke-resize-after 2 800 600 } finally { Pop-Location }
```

预期输出 `KUENGINE_GRAPH_RESOURCE_PROBE_READY`，并在第二次 compile 后仍可 resize；resize 输出 `KUENGINE_GRAPH_RESOURCE_PROBE_RESIZE_OK`，相对 image generation 增加、absolute buffer generation 保持。业务统计应为 **1 draw / 3 vertices**；无 Validation error。它验证 ParameterSet 候选发布/重编译释放顺序、internal allocation/resolve、显式 transfer/vertex/index/indirect/sampled use、同步2 barrier、attachment→sample、content-valid、relative image 重建与 absolute buffer 复用。

要用 Synchronization Validation 手动核对，须在启用 Vulkan Validation 的 Debug PowerShell 环境先设置 `$env:VK_LAYER_ENABLES='VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT'`，再按上述参数运行并检查 startup marker；最终 QA 的 Probe 为多帧 resize、errors=0，唯一 warning 为 enable 消息。开发 callback 时在 `declare` 声明资源 use/range，在 `prepare` 创建稳定 GPU 对象，在 `execute` 只使用 `GraphCommandContext` 的 checked fill/copy/compute/dispatch；native scope 只能声明已知 capability/side-effect 的子集。该示例不验证无 compute queue、失败注入、长时/多队列、视觉像素正确性或 RenderDoc。
