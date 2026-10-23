# 当前回归检查

核对2026-10-11：C最终QA ACCEPTED + DOCUMENTED，REQ-01～13首轮范围已验收，WP03/M4已由Lead最终审计闭环（DONE）、M5未开始。当前C整体验收见下节，自动回归明确继承冻结B同输入证据；A/WP01/02/M2/M1保留日期化历史，不把旧未运行项当最新阻塞，也不改写旧失败。

## 构建与 CPU 测试

前提为 VS2022 C++ 工具链、CMake 3.27+、Vulkan SDK 和仓库 vcpkg 工具链。在仓库根目录执行：

```powershell
cmake --preset vs2022-vcpkg
cmake --build --preset debug
ctest --preset debug
```

2026-10-24 QA fresh 工作区启用GPU smoke后注册40项（含M5在途工作区注册）；本包仅跑下文VolumetricIntegration定向范围。CornellBox在2026-10-21时注册33项，其定向范围见后文。B当时31项，独立fresh Debug/Release全build0，各core137/camera40；显式Debug SyncVal31Passed/0Skipped/0Failed actual0 190.13s，Release3Passed/28Skipped/0Failed actual0 12.17s。另20真实Release GPU groups逐组actual0；77/CTest skip非GPU通过，Release不捕获Validation。已有build命令须指同构建目录，不混preset。

GraphResourceProbe 的手动 Synchronization Validation 前置命令为：

```powershell
$env:VK_LAYER_ENABLES='VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT'
```

启动后先确认Validation startup，核Probe READY/COMPUTE_READY/RESIZE、1draw/3提交索引、蓝三角/真实resize与实际正常exit/error；marker不是画面证明。完整命令与原VK_LAYER_ENABLES保存/恢复见[GraphResourceProbe](graph-resource-probe-example.md)、[开发渲染特性](developing-render-features.md)。本页环境设置片段不是完整wrapper；实际复跑应用try/finally恢复调用者原值。

## 可选 GPU Smoke

### CornellBox / REQ-15 独立验收（2026-10-21）

正式 [QA报告](../../build/qa-cornell-box/qa-report.md) **ACCEPTED**。12个实现/测试/构建文件前后SHA256一致（`source-before.json` / `source-after.json`），QA无实现修复，文档同步期间代码继续冻结。此包独立于M5，不重开M4；以下只描述本包实际执行范围。

| 检查 | 实际结果与证据（相对 build/qa-cornell-box） |
|---|---|
| 独立配置/构建 | VS2022 x64/vcpkg；Debug CornellBoxApp/test_core/test_mclaren_camera/TriangleApp/ForwardReuseApp，Release CornellBoxApp/test_core，均退出0；configure.log、build-Debug.log、build-Release.log |
| Debug定向CTest | 7/7，0skip：core_tests（142）、mclaren_camera_tests（组合40）、Cornell默认与resize/recompile、Triangle、ForwardReuse及其recompile；ctest-Debug.log。当前注册33项未全跑 |
| Release CPU | core142/142，ctest-Release-core.log、Release-core-detail.log |
| Debug同步验证 | 显式VK_LAYER_ENABLES同步验证，Cornell两项复跑errors=0；ctest-Debug-cornell-detail.log，startup enable有记录。既有EOS重复Overlay注册warning保留 |
| 真实Release GPU | 默认Deferred4帧、resize/recompile6帧、Forward3帧，均actual0/SMOKE_PASS/matched；gpu-Release-{default,resize,forward}.log。Deferred10/114、Forward9/111，未依赖77skip，也未以FPS证明性能提升 |
| 独立原始GPU检查 | check_raw.py独立解析BaseMetallic/PositionModel/HDR half，用ray/plane/OBB验证最近表面；默认2499/resize1720样本，两配置通过，最大世界位置误差0.000980。红左绿右、PBRcode1、metallic0、两盒位置/遮挡和灯面HDR R>12通过；raw-{Debug,Release}-{default,resize}.json |
| 真实GUI | gui-Debug.log及当场工具原图观察：灯强1→0→1、曝光、两盒独立M/R、Base/Normal/Roughness/Metallic/ShadingModel/Scene、Deferred↔Forward保留、yaw/pitch/distance/Reset、侧栏收展；正常退出0/SyncVal errors0。未另存截图payload，不编造持久截图链接 |

运行命令、bat/preset目录和读回格式见 [CornellBox使用说明](cornell-box-example.md)。[final-default.png](../../build/qa-cornell-box/final-default.png) 来自实际GPU HDR经CPU Reinhard/sRGB转换，是场景预览，非Swapchain截图。读回仅为有限帧普通Deferred/PBR验证，容量2048²，非任意交互/路径/debug的通用截图。

NOT RUN：完整33项CTest/全部旧M4 GPU、RenderDoc、低能力设备、soak、额外FreeFly/minimize/focus stress、bat实跑（仅源码核对，等价CMake和应用命令已实际执行）。无强制检查阻塞，无需新增bugs。Release未启用Validation；四点光/常量环境仅演示既有简化PBR，无阴影、GI、真实面光积分或完整IBL。既有公共侧栏长提示横向裁剪/滚动不影响核心控件，QA未判定阻塞。

### M4-WP03-C 最终产品验收与证据边界

正式最终`build/qa-m4-wp03c-final/{qa-report.md,req01-13-qa-matrix.md,recipe-qa-audit.md,evidence-audit.md}`于2026-10-11 ACCEPTED。C Developer只读覆盖审计，无代码/Shader/测试/build定义修改；QA基于C276/B87 changed0与实际源码/API，新增独立Debug configure/build0/core137/composite40。composite40含18controller tests，不是40个camera-only。B两配置build0/core137/composite40、Debug显式SyncVal31/31(无skip)、Release CTest3pass/28skip77与另20真实Release GPU组actual0保持继承标签，不称C全部重跑。固定ROI/数值阈值、edited29states/16edits/layout×2/config和完整edited-Clear寿命证据继承不放宽、不改写成新SessionB。

| 当前会话 | 原始证据与范围 | 实际结果 |
|---|---|---|
| A3连续会话PID63840/window593128 | 原Q/session-a3 6图+recovery72图+final001–037；两个实例TRS/两slot全部标量/PBR-Unlit/保留、最终mixed gen19；两路径灯光/sky-env四组合/曝光/相机投影速度/八rawviews/统计收展与true native minimize/restore | 原launcher actual0，SMOKE3562615/error0；关闭输入未被final agent记录，不编造。无跨进程gen19保持声明 |
| 独立fresh-default SessionB PID80472/window264446 | final038–079；LoadA→Clear有效1×1黑GPU fallback→F/D/raw→真实2560×1369→1280×720→LoadB→badHDR保留；token1→2→3→4与release gpu_reset1。Model request5成功单节点replacement-model/selection0；badGLB request6保留新模型，camera/lights/HDR保留 | own actual0，SMOKE401459/error0；F070仍Pending，F071/raw marker才success；F033/F037不作resize证据 |
| Probe/Triangle/Cube/Alpha/ForwardReuse五新可视会话 | Q visual-* 13原图；实际输出、explicit SyncVal、resize/recompile、统计1/3、1/3、1/36、3/9、F9/45→D10/48 | 各actual normal0/error0；提交帧172516/79701/77538/86370/67260 |

final79原图与Q/recovery manifests SHA一致；七正常会话各有明确SyncVal startup、zero VUID/ValidationError/SMOKE_FAIL和完整清理，非stderr空白推断。历史A49/A2异常0xC000013A与blocked报告原样保留，不作正常pass、不拼接退出。用户HDR保持SHA25657EB0FD4BD5D9FE2FC7D23DA31933FFCCB2AB3BB371E7AC17734488BB901BFE0。warning1仅Vulkan捕获统计duplicate EOS loader；SyncVal enable startup是info。B模型另外七条缺TEXCOORD_0/1默认UV ModelLoader应用warning；badHDR/badGLB的CpuDecode/Compatibility错误是预期保留负例，不是Vulkan错误。Clear黑环境不等于整viewport全黑，直射模型/灰清屏有效。

独立实际相机证据：持续Orbit/release/wheel、pointer-only稳定、UI-origin drag隔离、W/S保持/释放、neutral FreeFly handoff/ResetView，且有同期用户确认。用户解释wheel时pitch变化是误按拖动，不是缺陷。用户明确“我已经自主测试了WASDQE，功能都正确，跳过这些检查”，并对右键持续转向/UI文本capture/失焦确认“已自测确认正确，免除重复人工检查”；这些功能标USER-SELFTESTED，重复人工SKIPPED，不称全部独立实机通过。原要求保留，不扩大豁免GPU/模型验收。

专门diagonal/startup/mode/reset/epoch-held物理stress、exact physical speed-rate新实机NOT RUN；18controller永久CPU测试证明数学/状态契约（包括delta/归一化/中性/epoch/capture），不证明物理事件投递/速率。普通true minimize/restore与真实resize有独立实机证据。低caps硬件/OOM/device-loss/forced live-format/soak/RenderDoc NOT RUN；IBL/OIT/HDR显示器/alias/multiQueue/多帧非首轮条件，M5未开始。

recipe QA核对现有Graphics/Compute/native/import/export APIs与所有权；Probe是固定128B fill/nativecopy/32uint compute-copy/sampledraw/postcopy，无host wholebuffer numeric readback，Compute prepare reset-then-build不是transactional恢复，native-after-execute lifetime仅source review、无专项永久误用测试。[接入说明](developing-render-features.md)记录当前方法，不承诺任意新算法已执行。无必要环境阻塞遗留，Lead最终审计已通过，M4闭环完成。

### M4-WP03-B 2026-10-10 独立局部验收与复现（C继承基线）

最新正式`build/qa-m4-wp03b/qa-fix-01/qa-report.md`于2026-10-10 ACCEPTED，仅B和QAB-01修复；Developer CODE_COMPLETE不是QA verdict。独立目录`build/qa-m4-wp03b/qa-fix-01/build`重新生成objects/libs/apps/checkers/SPIR-V，未借用Developer二进制；两配置build0/core137+camera40；Debug显式SyncVal全CTest31/31(skip0/fail0) actual0 190.13s；Release3pass28skip/fail0 actual0 12.17s(77非GPU pass)，另20实际Release GPU组均actual0。继承path8/200+配置CLI/eight rawMRT/UNORM-SRGB、HDR18/22、lights4、replacement7、六app resize/recompile、extras3，新增B两layout各29samples/16edits(58/config)。

永久入口 [run_mclaren_edits.cmake](../../tests/run_mclaren_edits.cmake) 使用 [两实例scene](../../tests/fixtures/viewer-two-instances.scene.json) 与 [edit_pixel_checks](../../tests/edit_pixel_checks.cpp)。fixture两实例共享同一个MeshHandle/GPU mesh，每实例3primitive/2slot(slot0两个primitive联动)。逐实例TRS/base/met/rough/emissive/PBR-Unlit-PBR，以及后TRS非空Unlit跨路径检查，复用实际UI request/Fence-safe publication而不是全局tint。每样本核对shared buffers/material resource/descriptor identities/no reupload，非环境操作HDR token/generation不变，未选slot/实例保持。actual completedSubmit配对不是frameIndex/Graph serial；Clear50→Forward52→GBuffer54→resize1280×720到800×600/recompile56→LoadB58→badedit60，脚本resize-after54。

checker不链接SceneData材质解析，采用独立fixture标量/顶点/model矩阵与投影几何1px侵蚀ROI/透视插值world-position参考；各primitive ROI非空，每次编辑受影响/未选ROI>20、HDR可见变化>.003；原scalar/world-pos容差`.002+.002*abs(ref)`，display RGB MAE≤1/255、P99≤3/255、Unlit max≤1/255不放宽。29states/16edits/layout、90180 affected ROI pixels，真实Unlit18/19比较10263RGB，最差MAE6.84085e-5/P99.000349760/Unlit max9.197e-5。普通全PBR比较的Unlit零样本不作Unlit通过证据。

独立`roi-audit.mjs`/`imported-viewport-metrics.json`按实际viewport侵蚀1px，不从图像选黑ROI或用整图最大值：LoadA/LoadB probe非黑，Clear23..26恰黑，failed HDR21==22与badedit27==28 raw bytes相同。完整1×1黑GPU候选发布后旧资源真正reset再记录token gpu_reset=1，不是只观察指针/path；正常GPU寿命与CPU可控候选失败不代表物理OOM/device-loss注入测试。

复跑使用独立目录，不覆盖正式QA/Developer证据。在仓库根完成前文`build/gpu-smoke`两配置构建后（路径按本机调整），例如：

```powershell
$editSourceRoot='D:/GraphcisBibile/Program/UGit/KuEngine'
$editBuildRoot='D:/GraphcisBibile/Program/UGit/KuEngine/build/gpu-smoke'
$env:VK_LAYER_ENABLES='VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT'
cmake "-DAPP=$editBuildRoot/bin/Debug/MclarenApp.exe" "-DCHECKER=$editBuildRoot/bin/Debug/edit_pixel_checks.exe" "-DWORKING_DIR=$editBuildRoot/bin/Debug" "-DSOURCE_DIR=$editSourceRoot" "-DEVIDENCE_DIR=$editBuildRoot/edit-evidence/Debug-local" -DREQUIRE_VALIDATION=ON -P tests/run_mclaren_edits.cmake
cmake "-DAPP=$editBuildRoot/bin/Release/MclarenApp.exe" "-DCHECKER=$editBuildRoot/bin/Release/edit_pixel_checks.exe" "-DWORKING_DIR=$editBuildRoot/bin/Release" "-DSOURCE_DIR=$editSourceRoot" "-DEVIDENCE_DIR=$editBuildRoot/edit-evidence/Release-local" -DREQUIRE_VALIDATION=OFF -P tests/run_mclaren_edits.cmake
```

默认REQUIRE_VALIDATION=ON；Release实际GPU设OFF，必须两layout app/checker真实0，不接受77作pass。脚本另检查五个显式scene初始化负例actual1，不当GPU通过；缺值/重复scene和metadata/range/prepare失败保留由CPU测试覆盖。完整Debug注册项可`ctest --test-dir build/gpu-smoke -C Debug -R gpu_mclaren_edits -V`（先设置SyncVal并核对startup）；若本机构建未开GPU选项，不会注册此项。

最终官方GUI只以`gui-final-03`同一完整会话为验收：自己的Debug runtime、绝对--scene/requireValidation、显式SyncVal，33未修改原JPEG的index SHA核对0 mismatch。两实例逐TRS/标量/Unlit禁用值保持/PBR恢复/slot联动及另一实例保持、path/MRT、sidebar/maximize-restore、LoadA→Clear→path/MRT/resize→LoadB→badHDR保持；末尾再核查双实例全值。保留Process.Handle→WaitForExit/Refresh记录真正GUI_ACTUAL_EXIT=0；launcher0不替代app。raw stdout SyncVal启用/1705200submitted/warnings1/errors0，stderr0、Validation Error/VUID扫描0；唯一warning为系统EOSOverlay重复loader，预期badHDR application error不是Vulkan错误。token1/2/3替换reset、token4 shutdown reset均gpu_reset1。87实现输入start/end changed0，33原图mismatch0；未改用户HDR。

历史保持：旧QAB-01 REJECTED在上层qa-report.md；恢复blocked另存`qa-report-blocked-2026-10-10.md`，final02 locked原图/recheck/检查点保留；后来final02 actual exit0仅关闭会话，不证明完整编辑。旧qa-fix-01 partialGUI后来0但止于Clear；原pre-fix fullGUI exit仍unknown、补充startup-close0不能替代。没有拼接旧截图/退出码为final03验收，没有删除失败历史。

B验收当时C完整输入/需求尚待，不以A/B sidebar/maximize或旧M1冒充C；最新C证据与USER-SELFTESTED/重复SKIPPED见前节。持续未运行：物理GPU OOM/fault/device-loss、低capability硬件、forced live-format、soak、RenderDoc；M5未开始。

### M4-WP03-A 历史局部验收与可复现矩阵

正式`build/qa-m4-wp03a/qa-report.md` ACCEPTED仅A，Dev交接`build/m4-wp03a-dev/wp03a-handoff.md`非QA结论。fresh两配置全build0，各CPU137/32，Debug显式SyncVal30/30无skip；ReleaseCTest3pass27skip另19真实GPU groups：路径8scenarios/200samples+两Source配置/CLI负例，HDR18cases/22checks，lights4，replacements7，六apps，extras3。Debug/Release都保留原HDR `.002+.002*abs(ref)`、1px侵蚀非空PBR/Unlit ROI MAE≤1/255/P99≤3/255/Unlit max≤1/255、rawMRT mapping≤1/255，不按结果放宽。

路径每scenario65提交帧、25捕获样本，六scenario另独立checker16映射+5跨路径比较；late resize两Source在submit12后变化，保持旧copy extent与actual completedSubmit配对（不是frameIndex/FIF/Graph serial）。clamp fixture实际position越界404352/emissive越界202176通道观测；曝光/Blend变更使SceneColor变化但debug/MRT不变。能力负例CPU覆盖，不外推低能力实体/alloc-fault。

官方真实GUI两例24张原图，非CLI冒充点击：ForwardReuse非默认背面preset，F→D→Normal→F、侧栏与实际maximize/restore；Mclaren手动FreeFly/FOV45.926/exposure1.747、F→D八views→Forward恢复、sidebar/resize，日志exit0/errors0。自动smoke非默认47/speed5.3/exposure1.7另行验证，不混同手动值。证据GUI index/log、path/HDR/light各Debug和Release-real目录及release-direct-matrix.log（最终groups19）；71实现哈希不变。失败历史保留在Dev交接，不将初次exit8/abort或错误typed invocation改写通过。

独立配置构建/全Debug验证（仓库根，工具链路径按实际安装调整）：

```powershell
cmake --preset vs2022-vcpkg -B build/gpu-smoke -DKUENGINE_ENABLE_GPU_SMOKE_TESTS=ON
cmake --build build/gpu-smoke --config Debug
cmake --build build/gpu-smoke --config Release
$env:VK_LAYER_ENABLES='VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT'
ctest --test-dir build/gpu-smoke -C Debug -V -j 1
ctest --test-dir build/gpu-smoke -C Release -V -j 1
```

当前公共Shader输出bin/$<CONFIG>/shaders实测隔离，aggregate path/HDR/light CTest evidence为各`*/$<CONFIG>`且配置验证runtime目录也独立。部分legacy individual resize/backface prefix仍共享；Release要求Validation时77在读回写入前，不声称所有prefix已隔离。自定义复跑必须独立EVIDENCE_DIR并保留verbose/逐case真实exit，构建目录报告不保证随Git分发。

永久脚本默认REQUIRE_VALIDATION=ON；Release真实GPU显式OFF，不接受skip77当pass。完整参数如下（仓库根，输出目录使用本地新复跑目录；变量为task-specific路径）：

```powershell
$pathSourceRoot='D:/GraphcisBibile/Program/UGit/KuEngine'
$pathBuildRoot='D:/GraphcisBibile/Program/UGit/KuEngine/build/gpu-smoke'
cmake "-DAPP=$pathBuildRoot/bin/Release/ForwardReuseApp.exe" "-DMCLAREN=$pathBuildRoot/bin/Release/MclarenApp.exe" "-DCHECKER=$pathBuildRoot/bin/Release/path_pixel_checks.exe" "-DWORKING_DIR=$pathBuildRoot/bin/Release" "-DSOURCE_DIR=$pathSourceRoot" "-DEVIDENCE_DIR=$pathBuildRoot/path-evidence/Release-local" -DREQUIRE_VALIDATION=OFF -P tests/run_viewer_paths.cmake
cmake "-DAPP=$pathBuildRoot/bin/Release/ForwardReuseApp.exe" "-DCHECKER=$pathBuildRoot/bin/Release/hdr_pixel_checks.exe" "-DWORKING_DIR=$pathBuildRoot/bin/Release" "-DEVIDENCE_DIR=$pathBuildRoot/hdr-evidence/Release-local" -DREQUIRE_VALIDATION=OFF -P tests/run_hdr_semantics.cmake
cmake "-DAPP=$pathBuildRoot/bin/Release/ForwardReuseApp.exe" "-DWORKING_DIR=$pathBuildRoot/bin/Release" "-DEVIDENCE_DIR=$pathBuildRoot/deferred-evidence/Release-local" -DREQUIRE_VALIDATION=OFF -P tests/run_deferred_light_semantics.cmake
```

切Debug exe/checker与Debug-local目录、REQUIRE_VALIDATION=ON，并设置上述SyncVal环境即可验证Debug独立矩阵。typed replacement入口避免CMake相对路径被PATH类型规范化；valid/bad/resize分别以DEFERRED OFF和ON执行，continuous以OFF（共7case）。例：

```powershell
cmake "-DKUENGINE_SMOKE_APP=$pathBuildRoot/bin/Release/MclarenApp.exe" "-DKUENGINE_SMOKE_WORKING_DIRECTORY=$pathBuildRoot/bin/Release" -DKUENGINE_MCLAREN_SCENARIO=valid -DKUENGINE_MCLAREN_DEFERRED=OFF '-DKUENGINE_MCLAREN_MODEL:STRING=models/props/mclaren_765lt.glb' '-DKUENGINE_MCLAREN_HDR:STRING=environments/hdr/citrus_orchard_road_puresky_4k.hdr' "-DKUENGINE_MCLAREN_BAD_MODEL:STRING=$pathSourceRoot/tests/fixtures/invalid-replacement.glb" "-DKUENGINE_MCLAREN_BAD_HDR:STRING=$pathSourceRoot/tests/fixtures/invalid-replacement.hdr" -DREQUIRE_VALIDATION=OFF -P tests/run_mclaren_replacement_smoke.cmake
```

六apps与extras逐命令/exit在QA `run-release-direct.ps1`/release-direct-matrix.log，app工作目录bin/Release；六例`--smoke-frames 5 --smoke-resize-after 2 800 600 --smoke-recompile-after-setup`。extra两路径`--hdr-fixture --deferred-readback-prefix <独立完整prefix> --smoke-frames 6 --smoke-resize-after 2 800 600 --smoke-recompile-after-setup`，Deferred添`--deferred`；backface `--deferred --forward-reuse-backface --deferred-readback-prefix <prefix> --smoke-frames 3`。检查每case actualexit0与matched/readback/retained markers，不只batch最后退出。

上述A验收当时B/C未完成；B局部与最新C最终证据分别见前节，不由A回溯提前证明。低能力实体/物理allocation failure/forced live-format/soak/RenderDoc仍未运行，M5未开始。

### M4-WP02 历史独立验收与永久HDR矩阵

正式ACCEPTED：`build/qa-m4-wp02/qa-report.md`；实现交接`build/m4-wp02-dev/wp02-handoff.md`只报CODE_COMPLETE，不替代QA。RTX4060Ti独立Debug/Release完整构建、CPU与DebugSyncVal29/29通过；ReleaseCTest3Passed26Skipped另26真实GPU+22checker、6补充回归。四真实GUI（两app×两路径）、侧栏collapse/reopen及实际maximize通过。普通ForwardReuse9/45或Deferred10/48，Mclaren91/9202884或92/9202887，readback两display probes额外2/6；既有Triangle/Cube/Alpha/Probe计数不变。

`gpu_hdr_semantics`执行两路径×9预设=18真实GPU cases/22checker：解析HDR/单层双层透明/opaque遮挡、曝光1/2、semantic front/backface、非平坦normal/UV1/texture-transform/normalScale1/0，以及天空/环境四组合。双路径resize/recompile、模型/HDR成功替换与失败保留、completed-submit统计由完整CTest/direct矩阵覆盖。Depth前后D32字节一致；曝光前HDR字节一致但Display变化；真实UNORM/manual和SRGB/hardware最大差≤1/255。

固定阈值不随结果放宽：HDR解析`abs(error)≤.002+.002*abs(reference)`；单层`(1.1875,.875,.875)`、远近双层`(.5625,1.125,.25)`、遮挡`(.1,.2,.3)`、HDR`(4,2,.5)`。最终RGB predefined ROI侵蚀1px并拒绝空PBR/Unlit集合，两路径MAE≤1/255、P99≤3/255、Unlit最大差≤1/255。实际analytic全部0；semantic front ROI24954/45436，MAE .0198797/255、P99 1/255、Unlit max1/255；backface ROI2485/1816，MAE .140742/255。非平坦normal响应5.60208/5.60134字节级，normal跨路径MAE .00416319/255，Unlit不变；environment响应68.3435/68.349，Unlit不变，四sky/env组合独立性通过。

常规复跑（先配置GPU smoke并构建）：

```powershell
$env:VK_LAYER_ENABLES='VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT'
ctest --test-dir build/gpu-smoke -C Debug -V -j 1
ctest --test-dir build/gpu-smoke -C Debug -R gpu_hdr_semantics --output-on-failure
```

WP02当时共用hdr-evidence导致Release skip覆写Debug首case文本；历史权威Debug为build/qa-m4-wp02/hdr-debug-final和debug-hdr-final.log，Release为release-direct目录/matrix。A已将aggregate证据按配置隔离，旧结果/失败不重写；部分legacy prefix仍共享，复跑独立EVIDENCE_DIR。构建目录证据不保证Git分发，永久脚本/fixture/checker在仓库。

独立目录完整矩阵复跑（仓库根，已构建示例/checker，按实际构建目录替换）：

```powershell
$env:VK_LAYER_ENABLES='VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT'
cmake -DAPP=D:/GraphcisBibile/Program/UGit/KuEngine/build/gpu-smoke/bin/Debug/ForwardReuseApp.exe -DCHECKER=D:/GraphcisBibile/Program/UGit/KuEngine/build/gpu-smoke/bin/Debug/hdr_pixel_checks.exe -DWORKING_DIR=D:/GraphcisBibile/Program/UGit/KuEngine/build/gpu-smoke/bin/Debug -DEVIDENCE_DIR=D:/GraphcisBibile/Program/UGit/KuEngine/build/hdr-local-debug -P tests/run_hdr_semantics.cmake
```

WP02脚本当时要求Validation；A已新增REQUIRE_VALIDATION=OFF永久真实Release入口，当前复跑用上节命令，skip仍非GPUpass。WP02原direct脚本/日志保留；checker encoding/extent/truncation和2048×2048/D32限制不变。

WP02当时未包含path UI/GBuffer/最终实例编辑/环境卸载；后续A/B与最终C证据见前节，低能力/live-format/soak/RenderDoc仍未运行。旧公共Shader风险在A按bin/$<CONFIG>落实隔离，WP02原记录不改写成提前通过。

### M4-WP01 历史局部验收与 Deferred 定向检查

最终证据为 `build/qa-m4-wp01/qa-final-report.md`，首次 `qa-report.md` 的 REJECTED 保留为历史发现；必要 `tests/run_deferred_light_semantics.cmake` 被 generic ignore 隐藏的问题已通过精确例外修复，最终 Debug SyncVal 窄复验3/3，Release skip protocol1 skipped/0 failed。构建目录报告不保证跟随 Git 分发，本节保留结果与可复现命令。

配置启用 GPU smoke 后，在同一构建目录运行：

```powershell
$env:VK_LAYER_ENABLES='VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT'
ctest --test-dir build/gpu-smoke -C Debug -R 'gpu_smoke_deferred|gpu_deferred_light_semantics' --output-on-failure
```

三项为 resize+recompile、backface 和 light semantics；后者使用仓库脚本运行 scene/zero/directional/point，要求实际 GPU `KUENGINE_DEFERRED_PIXELS_OK`、PBR 两种直接光响应、Unlit sum 不变，捕获 Validation/非零退出时失败，环境77转 `KUENGINE_DEFERRED_LIGHT_SKIP`。生成的 `deferred-evidence/*.ppm` 可人工查看；单独启动参数见 [ForwardReuse](forward-reuse-example.md)。

QA 使用 RTX4060Ti，确认 startup SyncVal enabled，全部检查 Debug errors=0。重复 compile 四 MRT generation=2，resize800×600后=3；Lighting rebind跟随变化。默认 Deferred8/36、Forward9/45、Mclaren91/9,202,884、Probe1/3。实际窗口检查侧栏 collapse/reopen及 maximize2560×1369，不额外侧栏rebind；当前 Forward/Mclaren画面回归通过。

独立 GPU readback/PNG 检查了 Empty/PBR/Unlit编码、Mask孔、深度重叠、MR/AO/emissive、背面及resize。960×540独立读回 Empty446350/PBR25646/Unlit46404；四灯光预设 Unlit输出sum均27015539，PBR scene/zero/directional/point sum为7290290/3849082/6937320/4766715，且独立内部Unlit ROI零像素差异。该结果是受控 fixture 的实际证据，不是全场景历史像素基线。

WP01当时未运行专用Deferred normal-map fixture，输出临时display-ready/排除Blend与天空；这些历史边界已由WP02真实normal/UV/HDR/Blend/天空检查与实现替代。当时完整路径/UI切换待WP03、旧Shader竞态未宣称解决；WP03-A现在已验收路径/UI与`bin/$<CONFIG>` Shader输出隔离，历史失败记录保留。强制live-format、低能力、soak、RenderDoc与完整held交互仍未运行。

GPU Smoke 默认不注册，避免没有可用 Vulkan、窗口系统或 Debug Layer 的开发环境把普通 CPU 测试变成失败。使用独立构建目录配置：

```powershell
cmake --preset vs2022-vcpkg -B build/gpu-smoke -DKUENGINE_ENABLE_GPU_SMOKE_TESTS=ON
cmake --build build/gpu-smoke --config Debug
ctest --test-dir build/gpu-smoke -C Debug -L gpu-smoke --output-on-failure
ctest --test-dir build/gpu-smoke -C Debug -L gpu-negative --output-on-failure
```

`gpu-smoke` 运行六个示例及受控预设的有限帧路径；Triangle 还在第 2 个成功提交帧后请求 960 × 540 resize。它要求 `KUENGINE_COMPLETED_STATS` 中的实际业务计数与 Pass 预期值匹配：Triangle 1/3、默认实心 Cube 1/36、Alpha3Pass 3/9；Mclaren 使用非零动态公式。`gpu_smoke_validation_injection` 是受控负例：应用必须报告 Validation failure，CTest 才视为通过。`gpu-negative` 的缺 Shader 负例要求业务初始化失败，不能误报为环境 skip。

| 结果 | 应用退出码/标记 | 含义 |
|---|---|---|
| PASS | 0 / `KUENGINE_SMOKE_PASS` | 达到有限提交帧与需要的 resize，且已捕获 Validation error 为 0 |
| FAIL | 1 或 `KUENGINE_SMOKE_FAIL` | 初始化、运行、帧数、resize 或其他业务失败 |
| Validation FAIL | 3 / `KUENGINE_SMOKE_FAIL reason=validation` | Messenger 捕获到至少一个 error；`KUENGINE_VALIDATION_ERROR` 是日志标记 |
| SKIP | 77 / `KUENGINE_SMOKE_SKIP` | Vulkan/Surface/设备不可用，或 `--smoke-require-validation` 但 Release、Layer、Debug Utils 不满足；不是通过 |

`KUENGINE_COMPLETED_STATS` 同时输出 completed submit 编号、draws、vertices、CPU 毫秒、GPU 状态（`unsupported` / `waiting` / `available`）以及预期值。只有 `available` 才有 GPU 毫秒；实体不支持 Timestamp 的 GPU 尚未在 M0 验证，不能将当前 RTX 4060 Ti 的结果外推到该情况。

可从 App 输出目录手动运行，例如：

```powershell
.\TriangleApp.exe --smoke-frames 5 --smoke-require-validation --smoke-resize-after 2 960 540
```

`--smoke-frames 0` 保持无限运行；`--smoke-inject-validation-error` 只用于受控负例，必须与 `--smoke-require-validation` 和正帧数一起使用。未知参数或不安全的 resize/injection 组合返回 2。

Mclaren 还支持专用替换参数，它们不属于 `EngineRunOptions`：

```powershell
.\MclarenApp.exe --mclaren-replace-model models/props/mclaren_765lt.glb --mclaren-replace-hdr environments/hdr/citrus_orchard_road_puresky_4k.hdr --mclaren-replace-after-updates 1 --smoke-frames 8 --smoke-require-validation
```

预期每个已完成替换输出 `KUENGINE_ASSET_REPLACE_SUCCESS`，最后的 completed stats 仍应与已发布模型匹配；失败路径输出 `KUENGINE_ASSET_REPLACE_FAIL retained=1`。这不模拟 OOM、上传故障或 device lost。

M2-WP03 的 GPU smoke 还包含 `ForwardReuseApp` 的语义、zero-lights、backface 三个 8/42 检查，以及 Mclaren valid/bad-retain/continuous/resize replacement；最终 Release 记录为 ForwardReuse 三预设各 8/42，Mclaren 90 draws / 9,202,881 vertices，Validation errors=0。Debug 验收还运行了 validation skip/negative 路径。人工抓帧确认 MR/AO/emissive、Mask 孔、Blend 深度、零光 PBR 变暗/Unlit 稳定、仅 double-sided 背面可见，以及 Mclaren 公共 Forward/两实例两 variants/bad retain。未运行 pixel readback 或实际鼠标 UI 自动化（CUA 不可用）；这些不能由受控预设替代。

可额外运行收起侧栏冒烟，确认 Runner 输出的 `KUENGINE_VIEWER_LAYOUT first/final` 合法且场景像素矩形没有溢出：

```powershell
.\TriangleApp.exe --smoke-frames 5 --smoke-require-validation --smoke-sidebar-collapsed
```

该开关只检查紧凑模式下的自动化布局契约，不能代替人工侧栏交互检查。

## 示例冒烟

从对应 exe 所在目录启动，并保持多帧运行，检查首帧、UI、resize、最小化和恢复。

| 示例 | 关键检查 | 业务计数预期 |
|---|---|---|
| Triangle | 颜色调整，稳定输出 | 1 draw / 3 vertices |
| Cube | 唯一右侧栏下左键场景拖拽、场景滚轮缩放、UI/侧栏不改变相机，实心/线框切换；最小化/恢复后再检查 | 1 / 36 或 1 / 24 |
| Alpha3Pass | 三形状顺序、颜色混合、后续节点保留前一节点颜色；展开 Parameters 中三个 Pass 的同名控件相互独立 | 全部启用时 3 / 9 |
| Mclaren | Scene/实例slot/相机/HDR/Clear/Load/path/GBuffer/viewport；最新C相机独立观察与USER-SELFTESTED/重复SKIPPED见前节 | 按phase精确汇总snapshot各实例indexed draw、可选Skybox、Display或GBufferDisplay1/3、DeferredLighting另1/3；默认F91/9,202,884或D92/9,202,887；fixture/Clear按实际构成，关Sky减1/3 |

计数只在Pipeline/资源就绪且实际记录绘制时成立，不含ImGui draw；模型顶点容量不同于索引提交数。以下为历史M1局部人工记录，不是本轮C：RTX4060Ti Debug四例唯一侧栏、紧凑/隐藏/重开、滚动/控件隔离/比例；Cube滚轮/UI隔离/最小化恢复/resize；Mclaren两模式切换、文本不移动/reset/收展/最大化最小化。这不是持续右键look/WASDQE六向/组合held的可靠实机证据，不等于C完整textcapture/focus/minimize验收，也未覆盖Win+D/物理非等比DPI。下列人工步骤是待复查操作方法，不声明本轮已执行。

## 侧栏与场景输入人工检查

1. 分别启动四例，确认只有右侧 `KuEngine Sidebar`；展开时检查 Performance、Parameters / Scene、Render Graph，缩小窗口后确认可垂直滚动。
2. 点击 Collapse Sidebar，确认紧凑 Performance 覆盖场景而场景恢复完整宽度；点击 Hide compact stats 后仍能用 Open Sidebar 重开。展开/收起期间场景比例正确，右侧不留黑缝。
3. 在 Cube 与 Mclaren 中，从场景区域按下左键后可持续旋转至松开；从侧栏或被 UI 捕获的位置按下不得在进入场景后开始旋转。滚轮只在场景内缩放。
4. resize、最小化并恢复后重复第 2、3 步。已验收的人工范围不包括 Win+D、物理非等比 DPI 或像素结果对比；这些项目不能标作通过。

## Mclaren 双模式相机检查

1. 在 Camera 的 `Mode` 中切换 Orbit Inspect / Free Fly，确认切换前后画面没有明显跳变；切换后先释放鼠标和 WASDQE，再开始下一次输入。
2. Orbit 模式确认左键场景拖动旋转模型、滚轮缩放；Free Fly 先从场景区域右键获得焦点，再检查 `W/S/A/D`、`Q/E`、右键视角、Move Speed 和两个 Reset 按钮的预期语义。
3. 点击/编辑侧栏输入框、窗口失焦或恢复后，确认没有移动或残留输入；折叠/重开和最大化/最小化后再检查模式、统计和 Validation 输出。
4. 最新C实际Orbit/W/S/释放/neutral handoff与用户自测WASDQE/right-look/UItext/focus、重复SKIPPED见前节。专项diagonal/startup/mode/reset/epoch-held stress与exact rates新实机NOT RUN，CPU契约不替代事件投递证明；这些操作方法不声明全部本轮独立执行。

## Graph 与时间统计

Graph Debug 面板应显示编译依赖和屏障计划，执行摘要包含已应用屏障、资源转换及 Rendering Scope。当前已不使用旧 skipped-in-rendering 开关作为正常执行路径。

FPS/Frame 是当前主循环样本；CPU/GPU/Draw/Vertices 是同一 completed submit。首个完成提交前显示 waiting/N/A；GPU 分为 Unsupported、Waiting、Available，有 Timestamp 且完成后才显示 GPU 时间。改变业务绘制负载后观察趋势，避免将 Present/VSync 的整帧限制误解为纯 GPU Shader 时间。

Debug 构建会请求 Validation Layer，并先检查 `VK_LAYER_KHRONOS_validation` 和 `VK_EXT_debug_utils` 是否可用。控制台的 `Vulkan instance created` 行会给出 requested、enabled 和 message capture 三个状态；缺少 Layer 或 Debug Utils 时会记录警告。Messenger 可用时，Vulkan general/validation/performance 消息以 `Vulkan <type> [<message-id>]` 写入 logger，error 还带有 `KUENGINE_VALIDATION_ERROR` 标记。

ApplicationRunner 会在 Engine 销毁后检查共享 Tracker，并把已捕获的 error 转为退出码 3；`--smoke-require-validation` 额外要求消息捕获能力可用。窗口存活或 Smoke PASS 只说明启动、有限提交、退出、指定 resize 和已捕获错误检查通过，不能替代人工画面、输入、最小化/恢复或像素正确性检查。

QA 对 M1-WP03 的自动证据包括 Debug/Release CPU 测试、Debug GPU Smoke 7/7，以及 Release 与缺 Validation Layer 的 skip/失败语义检查；这些不等于完整的连续人机输入覆盖。

历史M0/WP02公共Shader跨配置问题已在A按bin/$<CONFIG>落实隔离，旧失败保留。部分同步wait返回值仍未统一检查；aggregate配置隔离不代表所有legacy prefix隔离。最新C输入/自测/豁免/专项NOT RUN边界见前节。

## VolumetricIntegration / REQ-16 独立验收（2026-10-24）

正式 [QA报告](../../build/qa-volumetric-integration/qa-report.md) 为 ACCEPTED，文档已同步（DOCUMENTED），Lead 最终核对待完成。新包实现17文件冻结前后0 hash mismatch，git diff --check0；QA未修改实现。详细可复现命令和实际证据路径见 [使用说明](volumetric-integration-example.md)。

- fresh QA Debug/Release 六目标构建退出0，各core145/145、options4；Debug定向CTest8/8、Release3/3，均0skip，非完整40项全跑。
- 每配置11GPU场景/36实际提交：1/2槽固定时间同图、实际submit时间推进、密度/强雾积分能量次序、最低16步/零密度/零光、resize-recompile及尾帧通过。普通与读回均2draws/6vertices。
- Debug显式Synchronization Validation，errors=0，唯一既有EOS overlay loader warning；Release真实GPU退出0，不声明ReleaseValidation。
- 独立原作无噪声CPU参考5场景/890像素/2670通道通过，最大相对误差<0.097%；baseline645120像素原pow(1/2.2)PPM编码最大字节误差0。真实GUI暂停/参数/复位/侧栏及bat启动、app退出0、bat退出0通过。
- Triangle/CornellBox/ForwardReuse定向GPU回归通过；不扩大为旧GPU全部矩阵、M5整体或通用PBR体积效果验收。

NOT RUN：强制物理sRGB attachment、原站实时逐像素（原站/原纹理403）、RenderDoc、soak、完整旧GPU矩阵及其他硬件/OS。来源和CC BY-NC-SA3.0、替代噪声/固定机位差异见 [NOTICE](../../examples/volumetric_integration/NOTICE.md)。QA final-preview.png 是实际GPU PPM转换预览，不是Swapchain截图。
