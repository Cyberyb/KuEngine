# 开发渲染特性

核对2026-10-11。基于已实现的[GraphResourceProbe](graph-resource-probe-example.md)与最终C recipe QA审计；说明现有接入方式，不声称任意新特性已经运行。需求见[REQ-01、11～13](../structure/product-requirements.md)，范围见[路线图](../structure/roadmap.md)，契约见[调度](../design/02-render-pass.md)与[Graph资源](../design/12-graph-resources.md)。

## 接入位置与生命周期

从[Probe main](../../examples/graph_resource_probe/main.cpp)使用小型装配：配置`EngineConfig`，调用`runApplication(argc, argv, config, assemblyLambda)`；lambda通过`Engine::addPass<T>()`或`Engine::addCallbackPass(desc)`注册。Pipeline持有Pass与内部资源池，Runtime持有窗口/device/swapchain/单Fence帧循环和提交。算法不要复制这些职责，也不要修改中央Runtime循环。

对象Graphics覆盖`initialize(const RenderContext&)`、`setup(RenderGraphBuilder&)`、`execute(CommandList&, const FrameData&, RenderGraphResourceResolver&)`；按需实现`prepare(const RenderContext&)`、`onResize(width,height)`与`expectedFrameStatistics()`。compile依次initialize/setup→Graph编译与pool候选→prepare→发布。Shader、Pipeline、ParameterSet、Sampler由Pass/参数状态RAII持有；长期资产由Source持有，不由Graph附件池回收。

受限回调用[RenderGraphCallbackPass.h](../../src/KuEngine/Render/RenderGraphCallbackPass.h)的`CallbackPassDesc<Parameters>`：`name/declare/execute`必需，`prepare/nativeContract/expectedStatistics`可选。`declare(builder, Parameters&)`刷新typed handles，`prepare(context, Parameters&)`创建GPU对象，`execute(GraphCommandContext&, frame, const Parameters&)`只读参数。不要在UI或execute临时创建配置相关GPU对象。Probe Compute prepare先reset旧pipeline/parameters/shader再build，不是transactional live-recompile恢复；Draw initialize则完整候选成功后发布。

## 声明、记录与借用

- `createImage/createBuffer`声明pool-owned内部资源；`ImageDesc`含format/usage/absolute或swapchain-relative extent/aspect/clear/initialContent，`BufferDesc`含size/usage/memoryUsage/initialContent。同名kind/ownership/descriptor必须完全一致。注册顺序参与资源版本推断，不可随意交换。
- `useImage/useBuffer`声明每次读写的`ImageUse/BufferUse`及subresource/range；attachment用`colorAttachment/depthAttachment`的Clear/Load/Store。Undefined不能首读，LOAD需要已存储内容；资源流不足以表达时添加`dependsOn`。
- execute按准确use和包含于声明的range解析当前typed handle。resize/recompile后刷新handle；allocationGeneration或ImageView变化时重绑descriptor，不永久缓存裸allocation。Graphics使用匹配附件format的vertex/fragment Shader，设置场景viewport/scissor，离屏显示处理UV scale/offset。普通跨Pass屏障由Graph sync2计划记录。
- Compute prepare检查`graphicsQueueSupportsCompute()`，使用显式COMPUTE stage与匹配layout/storage bindings；execute用`bindComputePipeline/bindParameterSet/dispatch`。Compute/Transfer不得在Graphics attachment scope内执行；同Graphics Queue并非Async Compute。Draw统计是提交顶点/索引数，不是唯一顶点。
- Native callback先声明已知`NativeCommandCapability`与读/写`NativeCommandSideEffect`，execute向`nativeCommands(requestedSubset)`请求子集；通过scope解析合法资源/use/range，再用`scope.commandBuffer()`记录算法命令。scope move-only、context销毁后失效，不保留它或裸command buffer用于后续提交；任意裸Vulkan正确性不自动检测。旧对象是`TrustedLegacyObject`，不是checked callback。

外部资源先`importImage/importBuffer`声明准确descriptor，录制前以handle或同名`RenderPipeline::bindExternalImage/Buffer`绑定。`ExternalImageBindingInfo/ExternalBufferBindingInfo`传实际对象、尺寸/size/format/aspect/usage、`initialState/finalState/contentsValid`；image另有`clearValue/defaultLoadOp/defaultStoreOp`，layout位于初始/最终`ResourceState2`中，无平行layout字段。usage须是声明的完整超集。调用方保持GPU对象直到执行/提交完成并负责图外同步；Pipeline只借用，绝不分配/销毁import。Engine交换链/最终Present和示例staging→HOST_READ是已有参考，不隐式另行提交。

导出先在setup调用`builder.exportImage/Buffer`，成功execute后再取`RenderPipeline::exportImage/Buffer`并检查`valid(&error)`。token是frame/epoch与owner约束的借用view，不转移所有权、不延长GPU寿命；下一frame、resize/recompile、owner销毁即失效，allocation generation也须用于重绑。跨用途使用须重新显式import/bind准确状态，不把旧token当retirement handle。

## 具体参考链

| 节点 | 声明与算法 |
|---|---|
| ImageClear | `ProbeInternalColor`颜色附件Clear/Store→FragmentSampled，清为`(.12,.28,.82,1)` |
| BufferFill | `ProbeSourceBuffer`128B TransferDst：固定3vec2位置offset0、3uint索引offset32、indirect命令offset48 |
| native Copy callback | source TransferSrc→`ProbeComputeInput`TransferDst；checked Transfer scope拷贝128B |
| Compute callback | input ComputeStorageRead→`ProbeDrawBuffer`ComputeStorageWrite；local32/group1复制32uint |
| SampleDraw | internal image sampled、draw buffer Vertex/Index/Indirect、swapchain附件；indexedIndirect绘制蓝三角，1draw/3提交索引；声明image export |
| PostDrawCopy | dependsOn SampleDraw，draw TransferSrc→`ProbePostDrawCopyBuffer`128B TransferDst |

源码在[GraphResourceProbePass.cpp](../../examples/graph_resource_probe/GraphResourceProbePass.cpp)，shader在[本例shaders](../../examples/graph_resource_probe/shaders)，不是resources下的新目录。POST_BUILD编译三shader到本例bin/config的shaders；从该exe目录启动。该链是固定fill/copy，非复杂生成几何；PostDrawCopy没有host whole-buffer numeric readback。native-after-execute inactive/ensureActive为源码核对，未执行专项永久误用测试。

## 构建与现有Probe回归

在仓库根执行；若preset未配置先configure。保存并恢复调用者原`VK_LAYER_ENABLES`（包括原本不存在），不覆盖正式QA输出：

```powershell
cmake --preset vs2022-vcpkg
if ($LASTEXITCODE -ne 0) { throw 'Probe configure failed' }
cmake --build --preset debug --target GraphResourceProbeApp
if ($LASTEXITCODE -ne 0) { throw 'Probe build failed' }
$taskProbeHadEnables = Test-Path Env:VK_LAYER_ENABLES
$taskProbePriorEnables = $env:VK_LAYER_ENABLES
Push-Location build/cmake-tools/vs2022-vcpkg/bin/Debug
try {
    $env:VK_LAYER_ENABLES = 'VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT'
    ./GraphResourceProbeApp.exe --smoke-frames 8 --smoke-require-validation --smoke-recompile-after-setup --smoke-resize-after 2 800 600
    $taskProbeExit = $LASTEXITCODE
    "ACTUAL_APP_EXIT=$taskProbeExit"
} finally {
    Pop-Location
    if ($taskProbeHadEnables) { $env:VK_LAYER_ENABLES = $taskProbePriorEnables }
    else { Remove-Item Env:VK_LAYER_ENABLES -ErrorAction SilentlyContinue }
}
```

检查实际SyncVal startup、`READY/COMPUTE_READY local_size=32 groups=1/RESIZE_OK`、matched1/3、正常实际exit0/errors0；还要观察深色背景蓝三角与真实resize，marker/build0不是画面证明。77是环境SKIP而非GPU pass。C独立Probe实际蓝三角/2560×1369→restore/recompile、normal0/SMOKE172516/error0已验收；不称此命令本轮重新执行。原日志warning1为Vulkan计数中的duplicate EOS loader，startup enable消息是info；一般ModelLoader UV应用warning不归此计数。

新增特性后补对应CPU声明/range/native/import/export负例与真实GPU画面/统计/SyncVal/resize/recompile/exit证据，不能仅沿用Probe成功。永久契约测试在`core_tests`；完整构建/CTest与Release77/真实GPU区分见[回归](regression-checks.md)。

## 材质扩展定位

ShadingModel与RenderPath独立。新增模型时核对`Asset/ModelData.h`枚举/严格配置、Forward材质resolve/key/uniform/编码、shared material_sample/surface_lighting与Forward/Deferred Shader分派；明确GBuffer表示或Forward-only限制，未知模型拒绝，不改Runtime循环。C没有请求或实现新ShadingModel。完整IBL/OIT/HDR显示器/alias/multiQueue/多帧与M5不在本次首轮范围。
