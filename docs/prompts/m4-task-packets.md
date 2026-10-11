# M4 实施任务包

启动日期：2026-10-08。范围继承 roadmap 的 M4 和 REQ-01～13；本文件是执行任务包，不是当前设计或已验收声明。M5、多帧并行、IBL 预计算、资源别名与多 Queue 不在本轮范围。

## 协作门禁

Developer：GPT-6.1 Sol / high，唯一代码写者；QA：GPT-6.1 Sol / high，冻结实现后独立验收；Planner：GPT-6.1 Sol / medium，仅在 ACCEPTED 后同步文档。先读取各角色 Prompt 和 AGENTS.md。禁止自行提交、推送、删除用户文件或扩展范围。每包必须完成代码→验收→文档闭环，才能进入下一包。

## M4-WP01：最小 GBuffer 与 Deferred Lighting

执行状态：2026-10-08 QA ACCEPTED（最终 `build/qa-m4-wp01/qa-final-report.md`，首次 REJECTED 的脚本 ignore 交付问题已精确修复/复验）。已实现边界见 [Deferred 设计](../design/13-deferred-rendering.md)，验证与未运行项见 [回归说明](../usage/regression-checks.md)。本包验收不代表 WP02/03 或 M4 已完成。

- 需求：REQ-01、04、06、11、13。前置：M3-WP01～04 已验收。
- 目标：公共 GBuffer descriptor/布局及 ShadingModel 编码、Geometry Pass 和 Deferred Lighting Pass。Opaque/Mask 写 GBuffer；PBR 与 Unlit 明确分派，方向光/最多四点光沿用公共材质和灯光语义。Graph 管理 MRT、深度和输出的 allocation、同步与 resize。
- 共享边界：沿用 SceneData、ResolvedMaterial、ForwardDraw、GpuModelAsset 和 MaterialGpuResources；共用材质取样/UV/normal/AO/emissive 解释与可复用光照 Shader 逻辑，不复制资产加载或 Runtime。命名/窄组合可以调整，避免巨大中央 Renderer。
- 允许修改：src/KuEngine/Render、必要的 RHI MRT/format 验证、resources/shaders、示例装配、对应 CMake 与 tests。新增最小 Deferred 验证示例或在 ForwardReuse 加受控路径入口均可；业务集成不能只停留在无运行调用的 API。
- 本包不实现完整 UI 路径切换、Blend 合成、全项目线性 HDR 迁移（WP02/03）；为测试提供明确临时输出，标明限制。不能静默把 Blend 当 Opaque 写入 GBuffer。
- 验收：CPU 验证 formats/usages/encoding、MRT 与 depth、Geometry→Lighting 依赖、disabled producer、resize/rebind/recompile、PBR/Unlit/Mask 分派；Debug/Release build 与既有 CPU/Smoke 回归；真实 Vulkan + Synchronization Validation 运行新路径、resize/重复编译无错误；画面证据确认几何/Mask/Unlit、零光和方向/点光响应。检查材质与灯光一致性，构建成功不能替代 GPU/功能验收。
- 文档影响：design 01/02/04/09/10/11/12 及新增 Deferred 设计、current-architecture、需求状态、roadmap/实施计划、usage、实际当日日志。

## M4-WP02：共享材质、透明与线性 HDR 显示

执行状态：2026-10-08 QA ACCEPTED + DOCUMENTED。正式依据`build/qa-m4-wp02/qa-report.md`，当前事实见design11/13，命令/固定阈值/未运行项见usage回归说明；不代表WP03或M4出口完成。

- 前置：WP01 ACCEPTED 且 DOCUMENTED。
- 两路径共用线性 HDR SceneColor、天空背景、Forward Blend（对象级排序、depth test/no depth write）、曝光/色调映射/显示转换；UI 在显示转换后合成。Forward/Deferred 同一受支持输入画面近似一致。
- 验收：PBR/Unlit/Opaque/Mask/Blend/双面/环境可见与照明独立控制；HDR 保留大于 1 的值到唯一显示阶段，透明在线性空间合成；现有统计和 Graph 状态交接正确，resize/recompile/替换安全，GPU Validation 与画面核对通过。

执行细化（2026-10-08；以下契约已由WP02实现并独立验收）：

- Mclaren 与 ForwardReuse 均迁移。always-enabled ScenePrepare 在 UI 后生成稳定 snapshot，所有消费者显式依赖它；资产替换/容量准备仍在 Fence-safe update。Background CLEAR → Forward Opaque/Mask 或 Geometry/Lighting → Forward Blend → Display → Overlay，几何 Depth STORE，透明 LOAD/test/no-write。
- SceneColor 使用 RGBA16F，检查 color attachment/blend/sampled/transfer-src 能力；共享材质、灯光与资产，不重复上传。Opaque/Geometry 与 Blend 使用独立 UBO，或者一次准备后使用互不覆盖的稳定 draw 区域，禁止同帧反复写相同 offset 0。
- Display 是唯一曝光/tone/display 转换。UNORM 手动 sRGB，SRGB attachment 由硬件编码；保留明确的 gamma 开关限制。实际 Swapchain live-format 强制不要求，但受控离屏 UNORM/SRGB GPU 读回必须检查避免重复 gamma。
- 永久 GPU 解析 fixture：Unlit 背景 `(0.25,0.5,1)`、Blend 源 `(4,2,0.5)`、alpha `0.25`，线性期望 `(1.1875,0.875,0.875)`；容差 `abs(error) <= 0.002 + 0.002 * abs(reference)`。追加双透明层远到近、Opaque 遮挡与 no-depth-write 检查；曝光变化不得改变显示前 SceneColor。
- 同 fixture 两路径最终 RGB 在预定义可见面内 ROI（剔除 1px 边缘）比较：MAE ≤ 1/255、P99 ≤ 3/255，Unlit 最大差 ≤ 1/255。专用非平坦 normal-map/UV/normalScale 变体补足 WP01 残余；如 fixture 特性使阈值不适用，先报告并定义测试，不按结果放宽。
- 天空可见/环境照明四组合、双路径 resize/recompile/模型与 HDR 成功替换和失败保留、既有 CPU/Smoke/SyncVal/统计均核对。逐 Pass 计数仅作测试构成，不新增 M5 profiler。正式 UI/config 热切换与实例编辑留给 WP03。

## M4-WP03：路径切换、GBuffer 检查与产品闭环

执行状态：2026-10-11最终C QA ACCEPTED + DOCUMENTED；WP01/02/A/B既有门禁已闭，REQ-01～13首轮范围已验收。WP03/M4已由Lead最终审计闭环（DONE），M5未开始。下面任务定义保留功能范围，不把历史局部门禁误作当前阻塞。

- 前置：WP02 ACCEPTED 且 DOCUMENTED。
- 配置/CLI 和 UI 可选 Forward/Deferred，安全同步后切换且保留场景、材质、灯光、相机与侧栏状态；GBuffer 调试模式和不支持组合提示；补齐 REQ-01～13 明确要求的查看器操作（含实例选择、独立材质/TRS 编辑）及开发者接入说明。
- 验收：逐项需求证据矩阵；同场景两路径和调试模式、resize/侧栏/最小化恢复、模型/HDR 成功与失败保留、相机/UI 输入隔离、统计、GPU 同步与功能检查。未执行必要项不得标为通过。阶段出口需主 Agent 最终审计，M5 不开始。

### WP03 内部交付顺序

下表按实际门禁区分局部验收与待实现，任务定义不是直接design事实。每子包CODE_COMPLETE→QA ACCEPTED→Planner DOCUMENTED→Lead复核，拆包不减少WP03/REQ-01～13范围。

| 子包 | 范围 | 状态 |
|---|---|---|
| WP03-A | 配置/CLI/UI路径切换、GBuffer查看及直接验证 | 2026-10-08 ACCEPTED + DOCUMENTED；正式build/qa-m4-wp03a/qa-report.md，B/C不在此 verdict |
| WP03-B | 最终 Mclaren 查看器多实例独立编辑、真正 HDR 移除及验证 | 2026-10-10局部ACCEPTED + DOCUMENTED；B已由Lead复核闭环；正式build/qa-m4-wp03b/qa-fix-01/qa-report.md |
| WP03-C | 完整需求证据矩阵、交互补验和开发者闭环说明 | 2026-10-11最终QA ACCEPTED + DOCUMENTED；REQ-01～13首轮通过，最终Lead阶段闭环完成；C只读审计/验收无新代码，M5未开始 |

### WP03-A：配置、路径与 GBuffer

- 需求沿用 REQ-01、04～08、11、13，不新增或改窄需求。Developer 唯一拥有必要的 AssetConfig、Render/ScenePasses/Display、两例装配与 Source、Shader、CMake、tests；必要最小 Core 只读 completed-statistics 接口可加入。不修改文档、提交、推送或处理 B 的实例编辑/卸载实现。
- 独立 `RenderPath` 与 ShadingModel。SceneConfig 默认 Forward，JSON 严格验证；CLI `--render-path forward|deferred` 优先于 SceneConfig，保留 `--deferred` 兼容别名，冲突/非法值明确报错。两例均能使用公共路径控制；Mclaren 初始场景配置的路径字段必须实际生效，不仅 CPU parser 有字段。
- 两例 UI 可请求 Forward/Deferred；请求在下一 Fence-safe update 发布，不在 UI/execute 临时创建 Vulkan 对象。优先注册唯一命名的静态双分支，通过一致的 enabled-mask 切换；复用 Graph disabled-mask 内容/屏障预检，不复制 Runtime、不每次切换重载资产或重编译整个图。ScenePrepare 保持启用，消费者继续显式依赖，两个 Opaque/Geometry renderer 与 Background/Blend renderer 各自拥有独立 UBO。
- 切换保留 SceneSource、Mesh/Texture/Material/Environment 资源身份和 generation、材质参数、相机姿态/模式/FOV/速度、灯光、天空/环境开关、曝光、侧栏状态。正常切换不重复上传 Mesh/HDR。候选能力/初始化失败必须清晰报告并保留可用路径；无 Deferred 能力时不得静默冒充成功。resize/recompile 仍按当前生命周期安全重绑，不重置场景。
- 新增窄公共 GBuffer 显示能力，四 MRT 各提供 RGB/A 共八种视图：Base/Metallic、world Normal 映射 `n*.5+.5`/Roughness、world Position 明确固定范围映射/ShadingModel 固定色表、Emissive（明确截断范围）/AO。Empty 黑色；位置映射范围稳定且标注。直接读取 MRT 替代正常 Display，不读取混合后的 SceneColor，不受曝光、tone mapping 或透明颜色影响；只做一次 format-aware 最终显示编码，UI 仍最后合成。Forward 下禁用并解释 GBuffer 不可用，从 Deferred debug 切 Forward 时输出正常场景，不采样陈旧 MRT。
- GPU 对象/bindings 使用既有 RHI/Graph 参数与 allocation-generation 检查；保留 viewport/scissor/UV scale-offset 和侧栏布局，不引入通用编辑器/Profiler/多帧工作。
- 自动化请求复用真实 UI 发布路径。调度及读回归属使用实际 completed submit；`frameIndex` 是帧槽、Graph frameSerial 是录制生命周期，不能作为提交序号。必要 Runtime seam 只读且最小，示例侧有限验证逻辑，不把测试状态机迁入公共 Runtime。
- CPU 验证配置默认/合法/非法/CLI 优先级与冲突、分支 mask、Forward debug 拒绝/回退、依赖和不支持能力。真实 Debug SyncVal + Release GPU：两例多轮 F→D→F、状态/资源身份保留、resize/recompile、模型/HDR 成功与失败保留、统计；固定场景两路径 RGB 保持 WP02 阈值。八个 GBuffer 视图有实际 GPU 输出与独立参考映射检查，曝光/透明仅改变 SceneColor 时 debug 不变；验证 Empty、PBR/Unlit 编码及真实 UNORM/SRGB 编码，不能以 UI 文本或构建替代。读回有界，ROI/容差预先固定，不按结果放宽。
- 手动独立 QA 在官方工具中操作两例路径控件、Mclaren GBuffer 控件、侧栏与 resize，观察真实画面和状态，不只用 CLI 场景替代 UI。完整 held 相机/焦点/最小化恢复工作流仍由 C 补验，不据此提前 M4 通过。
- 保留 WP02 全量 CPU/GPU/负例/Shader 构建回归；新增永久脚本应正常 Git 可见、明确 Validation 77 skip 协议、独立配置证据目录。Developer 准确记录命令/退出码/未运行项，并将交接持久保存到独立 build 目录。

### WP03-B：最终查看器资产操作

前置已满足：A正式QA `build/qa-m4-wp03a/qa-report.md` ACCEPTED、Planner `planner-handoff.md` DOCUMENTED，Lead核对设计/usage/状态/图示、`git diff --check`与71实现输入hash不变。以下保留B执行/验收契约；当前实现见design，不能由契约反推实现事实。

历史B门禁快照（2026-10-10；C待派发/未验收只属当时，最新C最终状态见WP03-C）：B实际门禁2026-10-10：最新独立`build/qa-m4-wp03b/qa-fix-01/qa-report.md` ACCEPTED（旧QAB-01 REJECTED及blocked报告保留）；fresh Debug/Release build0，各core137/camera40；Debug显式SyncVal31/31 skip0/fail0 actual0 190.13s，Release3pass28skip/fail0 actual0 12.17s(77非GPU通过)，另20真实GPU组actual0。永久29samples/16edits/layout×2/config，独立几何/model/world-pos/1px ROI与非空Unlit10263RGB原阈值通过；Clear50/F52/GBuffer54/resize-recompile56/LoadB58/badedit60，真实GPU reset1。最终同一GUI33原图/实际app0/SyncVal error0，87输入冻结，warning1为系统EOSOverlay重复loader。Planner DOCUMENTED；B已由Lead复核闭环，C未派发/未开始；REQ/WP03/M4整体未验收、M5未开始。物理OOM/device-loss/低caps/forced live-format/soak/RenderDoc及C完整held/text/focus/minimize NOT RUN。

- 实现 REQ-02、03、08 已有口径：`--scene <scene.json>` 显式入口失败报错，不静默退回默认；提供两个 SceneInstance 共享同一 MeshHandle/GPU mesh、多 primitive/material fixture。不是 glTF 内部节点或 ForwardReuse 控件冒充最终查看器实例操作。
- 保留编辑所需 primitive/material/bounds 元数据与 instance/material 索引，释放大的 vertex/index/texture pixels；不能为 scalar/TRS 编辑重新上传 Mesh。实例选择控制该实例 TRS；material slot 选择控制该实例该槽的 base/metallic/roughness/emissive/PBR-Unlit，primitive 可用于定位槽，明确同槽哪些 primitive 受影响。另一实例不变，Unlit 不适用参数禁用但保留原值。有限 pipeline keys/容量在安全候选阶段准备；失败不发布编辑。
- 真正 ClearEnvironment：独立请求/按钮而非空路径或贡献开关；Fence-safe 构建同 schema 的有效 1×1 黑色 HDR GPU 回退，完整成功后发布、清空 active path、更新 generation、关闭 sky/env，释放旧导入环境 GPU 资源；失败保留旧状态。不能置 nullptr/悬空 descriptor。验证 LoadA→Clear→两路径/resize/recompile→LoadB，资源释放有明确寿命证据，不仅比裸指针。保留模型/相机/灯光；用户 HDR 文件不删除。
- 在真实 UI 和同发布路径的永久 GPU 验证中覆盖独立编辑、共享资源身份、两路径状态保留、替换失败和清空后继续加载。

#### B 所有权与实现边界

- Developer唯一代码/测试写者：`examples/mclaren`（Source/SceneAsset/Replacement/CLI与必要窄编辑、验证辅助文件）、必要公共`src/KuEngine/Asset/Scene`/`Model`/`AssetConfig`、`Render/PBRCommon`/`PBRResources`/`ForwardCommon`/`ForwardProgram`/`ForwardRenderer`的窄接口与实现、对应CMake/tests/fixtures及`.gitignore`精确例外。公共数据/验证保持无ImGui依赖；UI与有限自动化仍在示例，不引入ECS、通用编辑器或公共测试状态机。修改其他模块需先说明必要性给Lead，不复制Runtime/Shader算法，不进入C/M5。
- `--scene`由Mclaren参数解析明确剥离并传递到实际加载入口；缺值、重复冲突、不存在/非法JSON、引用模型错误必须准确失败。显式输入不退回默认车/默认场景；资源相对路径有明确根规则。无此参数的既有默认启动兼容。fixture使用至少两实例指向同一模型路径，验证SceneLoader去重后MeshHandle/GPU mesh共享，模型本身包含多个primitive与material slot。
- 保留轻量CPU材质、primitive/index range、bounds及实例元数据；只释放大块vertex/index/各纹理pixels/HDR pixels。`releaseCpuMesh()`当前清整MeshData，须重建合适释放边界。DrawRecord必须能反查instance/primitive/material slot；编辑不能仅改变全局颜色或缓存draw而丢失SceneData语义。
- 编辑请求只在UI排队，下一Fence-safe update校验并发布候选。TRS有限、正scale等继承现有契约；材质scalar范围明确，错误instance/slot/非法数值失败保留原状态。PBR/Unlit切换与base/metallic/roughness/emissive均作用于选定实例选定槽；同槽primitive联动须可理解，另一实例与其他槽不变。Unlit禁用不适用控件而保留值。标量/TRS编辑复用已有mesh与纹理descriptor，不再上传Mesh或HDR；有限pipeline keys与所有phase容量安全准备，准备失败不发布候选。普通路径/resize/recompile保留已编辑数据及选择。
- ClearEnvironment是显式操作类型，不复用空路径Load或仅关贡献；构造有效1×1黑色HDR完整GPU候选并复用现有environment descriptor schema。成功才更新空active path/generation/sky-env开关，旧资源在安全点真实销毁；失败保留旧环境和全部状态。允许窄寿命观察token/事件作为示例验证证据，不新增M5通用profiler。Clear后继续两路径/GBuffer/resize/recompile并可加载另一个有效HDR。若显式scene不含环境，定义并说明有效fallback语义，不能留下null descriptor；有无效显式HDR引用不可悄悄用另一个导入文件冒充成功。

#### B 验收门禁

- CPU：严格scene/CLI及失败、共享MeshHandle、释放大payload但编辑元数据有效、instance/slot定位/独立覆盖/同槽primitive、TRS/scalar校验、PBR-Unlit值保留、编辑失败不发布、Clear独立请求/状态generation及失败保留。测试不得靠增加全局编辑开关冒充独立编辑。
- 两配置全量构建/CPU；完整Debug显式SyncVal回归（含A所有30项，新增按实际计数）；Release CTest skip77单列，另外真实Release GPU。永久有界验证复用实际UI请求与Fence发布、actual completedSubmit；两实例多槽fixture覆盖逐实例TRS/base/metallic/roughness/emissive/PBR-Unlit，核对共享GPU身份与无重复上传、未选实例/槽不变、状态与像素变化。编辑后的同场景两路径保持既有固定ROI/阈值，不能按结果放宽；可复用A rawMRT与HDR证据读取工具。
- LoadA→Clear→Forward/Deferred/GBuffer→resize/recompile→LoadB，包括模型/HDR成功及失败保留。读回/独立参考确认Clear有效黑fallback与天空/环境贡献关闭、重新导入恢复；旧导入环境GPU资源释放有实际寿命证据，而非仅比较裸指针或活跃path。用户HDR文件不删除/改写。候选GPU内存故障无实机注入时明确未运行，可用CPU可控失败证明发布契约，但不假称OOM实测。
- QA以官方UI工具实际启动`--scene`fixture，选择两个实例/至少两个material slots、逐实例TRS与材质及ShadingModel编辑、检查Unlit控件禁用和另一实例不变；操作Load/Clear/Load、路径控件及侧栏/resize，保留真实截图/原始SyncVal日志/退出码。CLI自动化不冒充鼠标操作。完整held相机/text/focus/minimize仍归C，不提前验收REQ整体。
- Debug/Release、evidence/runtime配置目录隔离，正常Git发现脚本/fixture/checker；每条命令真实exit，失败历史不抹去，Developer持久`build/m4-wp03b-dev`交接后只报CODE_COMPLETE并冻结。QA新目录独立验收；通过后Planner同步design00/02/04/05/07/08/10/11/13及实际受影响其他文档、当前宏观架构/稳定REQ/路线/usage/同日日志。Lead关闭B后才进入C。

### WP03-C：完整产品闭环

执行状态：2026-10-11最终`build/qa-m4-wp03c-final/qa-report.md` ACCEPTED + DOCUMENTED。Developer覆盖审计未发现需修复代码缺口，C无实现修改；完整矩阵、现有接入recipe与七个正常可视GPU会话有最终证据。REQ-01～13首轮已验收，WP03/M4已由Lead最终审计闭环（DONE），M5未开始。

最终人工程序修订：用户明确自测WASDQE全正确并跳过重复；右键持续转向/UI文本capture/失焦自测正确并免重复人工，标USER-SELFTESTED/重复SKIPPED。独立持续Orbit/释放/UI-origin/W/S仍分列，普通true minimize/restore与真实resize另有证据。专门diagonal/startup/mode/reset/epoch-held stress/exact rates新实机NOT RUN，18controller测试仅CPU数学/状态契约；不删除以下功能要求或扩大到GPU/模型验证豁免。证据与限制见[回归](../usage/regression-checks.md)，现有开发流程见[接入说明](../usage/developing-render-features.md)。

- 逐项建立 REQ-01～13 的输入、操作、期望、源码入口、实际测试/GUI/图像证据矩阵，继承已有证据但不以局部通过证明整个需求。
- 补齐最终 Mclaren 两实例、多材质、模型/HDR 导入→材质/路径→灯光/相机→GBuffer→统计工作流；双模式相机真实长按拖动/WASDQE 六向/右键转向、UI/text/focus 输入隔离、侧栏收展、resize、最小化恢复。必要项无法执行则 VERIFICATION_BLOCKED，不虚报通过。
- 完整回归六示例、Graph Graphics/Compute/native capability 与 checked access、统计提交口径、同步与资源寿命；同步开发者接入操作及全部当前 design/structure/usage/日志。首轮不要求完整 IBL、OIT、HDR 显示器、别名、多队列或 M5 并行帧。
- 仅主 Agent 在所有子包门禁和整体验收审计通过后报告 M4 DONE，并停止，不开始 M5。任何未验证的必要项仍保留为未完成。
