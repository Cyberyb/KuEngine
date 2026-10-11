# GraphResourceProbe 示例

核对日期：2026-10-11。`GraphResourceProbeApp`顺序注册ImageClear、BufferFill、native Copy callback、Compute callback、SampleDraw、PostDrawCopy；统计1draw/3提交索引。固定3vec2/index/indirect fill→128B native copy→32uint compute copy→sample draw→postcopy，画面为深色背景上的蓝三角；非复杂生成几何，也无host whole-buffer numeric readback。完整接入步骤见[开发渲染特性](developing-render-features.md)。

从preset输出目录启动，保存并恢复原SyncVal环境：

```powershell
cmake --build --preset debug --target GraphResourceProbeApp
$taskProbeHadEnables = Test-Path Env:VK_LAYER_ENABLES
$taskProbePriorEnables = $env:VK_LAYER_ENABLES
Push-Location build/cmake-tools/vs2022-vcpkg/bin/Debug
try {
    $env:VK_LAYER_ENABLES = 'VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT'
    ./GraphResourceProbeApp.exe --smoke-frames 8 --smoke-require-validation --smoke-recompile-after-setup --smoke-resize-after 2 800 600
    "ACTUAL_APP_EXIT=$LASTEXITCODE"
} finally {
    Pop-Location
    if ($taskProbeHadEnables) { $env:VK_LAYER_ENABLES = $taskProbePriorEnables }
    else { Remove-Item Env:VK_LAYER_ENABLES -ErrorAction SilentlyContinue }
}
```

预期`KUENGINE_GRAPH_RESOURCE_PROBE_READY`、`KUENGINE_GRAPH_COMPUTE_READY local_size=32 groups=1`，第二次compile后仍可resize；`KUENGINE_GRAPH_RESOURCE_PROBE_RESIZE_OK`表示relative image generation增加、absolute buffer generation保持。核实际SyncVal startup、matched1/3、正常exit0/errors0并观察蓝三角和真实resize，marker/build不替代画面。77为SKIP不是GPU通过。Draw initialize候选发布；Compute prepare reset-then-build不是transactional恢复。

最终C QA独立Probe可视会话（最终报告2026-10-11、原日志`build/qa-m4-wp03c/visual-probe`）normal0/SMOKE172516/errors0，实际2560×1369 resize/rebind及restore/recompile、3原图完整。Vulkan统计warning1为duplicate EOSOverlay loader，SyncVal enable startup为info，不是所有应用warning总数。callback/native/import/export所有权与失效见[接入说明](developing-render-features.md)，native-after-execute lifetime仅源码核对、无专项永久误用测试。无whole-buffer host数值读回、低能力实机/OOM/device-loss/长时/多Queue/RenderDoc未运行；REQ首轮与M5边界见[需求](../structure/product-requirements.md)、[路线](../structure/roadmap.md)。
