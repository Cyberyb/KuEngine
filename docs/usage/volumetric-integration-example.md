# VolumetricIntegration / Shadertoy XlBSRz

2026-10-24：REQ-16 独立 QA **ACCEPTED**，文档 **DOCUMENTED**，Lead 最终核对待完成。示例位于 [examples/volumetric_integration](../../examples/volumetric_integration/README.md)，[当前设计](../design/04-triangle-example-tech.md) 描述所有权与数据流。

## 场景、来源与差异

复刻 [SebH / Sébastien Hillaire 的 VolumetricIntegration（XlBSRz）](https://www.shadertoy.com/view/XlBSRz)：红色两侧、绿色地面、蓝色背墙的开口房间，固定相机 (20,18,-50)，移动暖点光，constant/height/sphere 介质和改进的按段解析散射积分。默认视线100步、光方向16步；没有Cornell两盒、opaque interior对象或独立opaque阴影图。表面Lambert受同一介质透射衰减。

原站及原iChannel0纹理访问返回403，未运行原站实时逐像素对照。源码取自 Vipitis/shadertoys-dataset 的 commit `7a5d34083b03501a4d42238bbe65183b0b531f00`、`data/annotated/20k_2015-08.jsonl`、记录XlBSRz（归档获取日期2021-10-01）；解码原GLSL SHA256为 `696eda6d9c36df3c053b411eb25a0e3308243e46bbef54fccf830cebb32cedb9`。改编Shader保留作者，采用原记录 **CC BY-NC-SA 3.0**（署名、非商业、相同方式共享），精确来源与许可见 [NOTICE](../../examples/volumetric_integration/NOTICE.md)。

原噪声替换为自包含256² repeat/bilinear hash grid，细节纹理不同；固定相机不提供原iMouse位移。另作点光奇点/低步数未命中防护、Vulkan Y适配和分离显示。该例是场景/算法复刻，不承诺原站逐像素一致或通用Shadertoy播放能力。

## 构建与启动

在仓库根目录，用已配置的VS2022/vcpkg/Vulkan SDK工具链：

```powershell
cmake --preset vs2022-vcpkg
cmake --build --preset debug --target VolumetricIntegrationApp
Push-Location build/cmake-tools/vs2022-vcpkg/bin/Debug
./VolumetricIntegrationApp.exe
Pop-Location
```

或者在仓库根目录运行：

```powershell
./examples/volumetric_integration/run_volumetric_integration.bat Debug
# Release 也可；自定义构建目录用绝对路径
$env:KUENGINE_BUILD_DIR='D:/GraphcisBibile/Program/UGit/KuEngine/build/qa-volumetric-integration'
./examples/volumetric_integration/run_volumetric_integration.bat Release
Remove-Item Env:KUENGINE_BUILD_DIR
```

该bat默认构建目录为 `build/cmake-tools/vs2022-vcpkg`，前台启动并返回应用退出码。直接运行exe必须进入对应binary目录，让相对SPIR-V路径可用。三个新Shader通过OUTPUT/DEPENDS增量构建，输出按Debug/Release隔离到 `shaders/volumetric_integration`。

## 交互与命令行

默认1280×720窗口、2帧槽；侧栏打开时保留场景区域，固定相机投影随场景比例变化。QA默认实际读回场景为896×720；resize为960×640窗口时场景为672×640，不能把窗口尺寸当作导出裁剪尺寸。

侧栏支持Pause、Time、Integration、Fog density、Strong height fog、Light intensity、View steps、Shadow steps、Fog noise、Volumetric shadows和Reset reference defaults。Split左侧basic、右侧analytical，增加strong fog更容易观察散射积分差异。默认step=0时Pause冻结时间；Reset恢复动画time=0、analytical、density/light=1、strongFog=0、100/16步及noise/shadows开启。相机不支持鼠标Orbit/FreeFly。

| 示例参数 | 默认 / 允许范围 |
|---|---|
| `--volumetric-time T` | 固定时间0..1000000秒；不指定则动画 |
| `--volumetric-time-step T` | 每实际提交增加0..100秒，非零时要求固定time |
| `--volumetric-integration MODE` | `improved`（默认）、`basic`、`transmittance-first`、`split` |
| `--volumetric-density N` | 1 / 0..20 |
| `--volumetric-light N` | 1 / 0..10 |
| `--volumetric-strong-fog N` | 0 / 0..10 |
| `--volumetric-steps N` | 100 / 整数16..256 |
| `--volumetric-shadow-steps N` | 16 / 整数1..64 |
| `--volumetric-no-noise` | 关闭高度雾噪声 |
| `--volumetric-no-shadows` | 关闭介质阴影积分 |
| `--volumetric-readback-prefix PATH` | 可选导出，必须同时指定正数smoke-frames |

上述CLI范围与侧栏滑块范围可以不同。time-step用于有界回归，按 `time + timeStep × (submittedFrame - 1)` 计算；非零时即使Pause选中，有效时间仍随实际提交递增。缺值、非有限数或越界参数失败退出；参数可与公共 `--frames-in-flight 1|2`、有限帧/resize/recompile/Validation选项组合。

## 有界像素导出

从exe目录执行，输出prefix建议使用绝对路径：

```powershell
./VolumetricIntegrationApp.exe --smoke-frames 4 --frames-in-flight 2 `
  --volumetric-time 1.25 --volumetric-readback-prefix C:/temp/volumetric/reference
# 测试每次实际提交的动画变化：另加 --volumetric-time-step 0.5
```

每个实际提交输出 `_submitNNNNNN_slotS_WIDTHxHEIGHT` 后缀：`.rgba16f`为裁剪场景little-endian交错RGBA half线性辐射值（无文件头，尺寸见txt），`.ppm`为原pow(1/2.2)gamma预览，`.txt`记录submit/slot/尺寸/time/参数/hash/radianceSum。元数据hash为实现局部64位hash；回归checker另对真实raw字节计算SHA256。内容来源是Graph的线性SceneColor，不含UI，也不是Swapchain截图。

Scene→Display→可选Readback，数据从SceneColor分别流向Display和Readback；UI在Graph后合成。读取前等本槽Fence，resize/recompile/退出drain全部pending，包括尾帧；析构期失败也使应用返回失败。staging容量限制为整个附件总像素不超过2048×2048，普通渲染不受此限制。无限帧读回启动前即被拒绝。普通与读回运行均2draws/6vertices（UI不计）。

显示曝光1、无tone map，保留原pow(1/2.2)；UNORM直接写encoded值，sRGB附件先inverse-sRGB再硬件编码。未将该专用显示改成公共PBR显示契约。

## 独立QA与复现

正式结论见 [qa-report.md](../../build/qa-volumetric-integration/qa-report.md)，开发/Shader增量证据见 [handoff.md](../../build/volumetric-integration-dev/handoff.md)。QA使用独立构建目录、RTX4060Ti，实际命令：

```powershell
cmake --preset vs2022-vcpkg -B build/qa-volumetric-integration -DKUENGINE_ENABLE_GPU_SMOKE_TESTS=ON -DVCPKG_INSTALLED_DIR=D:/GraphcisBibile/Program/UGit/KuEngine/build/vcpkg_installed
cmake --build build/qa-volumetric-integration --config Debug --target VolumetricIntegrationApp test_core test_volumetric_options TriangleApp CornellBoxApp ForwardReuseApp -j 8
cmake --build build/qa-volumetric-integration --config Release --target VolumetricIntegrationApp test_core test_volumetric_options TriangleApp CornellBoxApp ForwardReuseApp -j 8
$env:VK_LAYER_ENABLES='VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT'
ctest --test-dir build/qa-volumetric-integration -C Debug -R '^(core_tests|volumetric_options|gpu_smoke_volumetric_integration|gpu_smoke_triangle|gpu_smoke_cornell_box|gpu_smoke_cornell_box_resize_recompile|gpu_smoke_forward_reuse|gpu_smoke_forward_reuse_recompile)$' --output-on-failure
ctest --test-dir build/qa-volumetric-integration -C Release -R '^(core_tests|volumetric_options|gpu_smoke_volumetric_integration)$' --output-on-failure
```

GPU测试开启时额外要求Python3；[checker](../../tests/run_volumetric_checks.py)只用标准库，每次新建run-*证据目录。Debug用required Validation、Release用optional；实际失败不会作为77通过。

- 两配置六目标build退出0，各core145/145、options4；Debug定向8/8、Release3/3，均0skip。QA配置实际注册40项，本包未全跑；旧Cornell33项等历史数字不改写。
- 每配置11GPU场景/36实际提交，1/2槽固定time同图、submit动画、density、强雾能量顺序 transmittance-first < analytical < basic、min16、zeroDensity、zeroLight、resize/recompile/尾帧全部通过。Debug显式SyncVal errors=0，唯一既有EOS overlay loader warning；Release真实GPU退出0，不宣称ReleaseValidation。
- 实际证据：`build/qa-volumetric-integration/tests/volumetric-evidence/Debug/run-0c3lrsiw` 与 `Release/run-45tcjb98`。独立无噪声原作CPU参考5场景/890像素/2670通道通过，最大相对误差<0.097%；全部baseline645120像素PPM编码字节误差0。参考脚本和结果保留于QA目录。
- 真实GUI全部关键控件、侧栏收展/比例与bat启动通过；app实际退出0、bat退出0。Triangle/CornellBox/ForwardReuse定向GPU回归通过。实现17文件前后hash0mismatch、QA diff-check退出0。

[final-preview.png](../../build/qa-volumetric-integration/final-preview.png)是实际GPU PPM转换预览；原始half是线性像素主要证据。NOT RUN：原站实时逐像素、原纹理、强制物理sRGB attachment、RenderDoc、soak、全部旧GPU矩阵、其他硬件/OS和物理OOM/device-loss。新例1/2槽通过不等于M5整体验收；M5-WP01仍IMPLEMENTING。
