# 历史与验收记录

这里供追溯，普通使用从[文档首页](../../README.md)进入即可。历史原文描述当时状态，不作为当前接口/进度依据；当前事实见[架构](../../design/README.md)，进度见[路线图](../roadmap.md)。

<a id="backup"></a>
## 整理前完整原文

[2026-10-28文档备份](docs-before-simplification-2026-10-28.zip) 保存整理前71份docs Markdown及8份外部入口/角色配置，共79项。条目保留仓库相对路径与原始字节，包含当时未提交的文档修改。解压到独立目录即可按旧结构查阅，恢复时先对比当前工作区，不覆盖其他修改。

旧14份模块设计、11份独立操作/回归说明、架构/目标/实施计划、角色索引等已合入6份日常文档；长QA流水和历史状态仍可在该zip按原路径查阅。现存历史文件只机械修正已迁移链接，未按新实现改写日期/验收事实。

<a id="history"></a>
## 历史快照与每日简记

- [Structure_725](../Structure_725.md)、[Structure_726](../Structure_726.md)：原日期Markdown；[725 PDF](Structure_725.pdf)、[726 PDF](Structure_726.pdf)为当时导出，未随整理更新。
- [旧v0.2执行计划](v0.2-execution-plan.md)、[发布记录](v0.2-alpha-release-notes.md)、[回归流程](v0.2-regression-checks.md)、[未完成草稿](Structure_temp.md)。
- [原架构PNG](kuengine-architecture.png)：迁移前所有权/执行描述，未重新绘制。
- [每日简记目录](../../logs)：日期文件原位保留；当天合并，一段概述加模块更新，不承载长期计划。

<a id="tasks"></a>
## 旧任务包

[M4任务包](tasks/m4-task-packets.md)、[M5双帧任务包](tasks/m5-dual-frame-task.md)保留原范围与门禁。它们不是当前用户授权；恢复开发需结合当前代码、路线图和新任务包。角色操作规则位于 [.codex/prompts](../../../.codex/prompts)，项目共同约定见 [AGENTS](../../../AGENTS.md)。

<a id="verification"></a>
## 已有验收证据

下列报告位于本地build目录，可能不随Git克隆；完整旧说明/报告路径与数字已在上面的zip保留。它们是历史运行结果，本次文档整理未重跑。

| 日期 / 范围 | 原证据入口 | 结论边界 |
|---|---|---|
| 2026-10-11 M4最终C | [正式QA](../../../build/qa-m4-wp03c-final/qa-report.md) | REQ-01～13首轮接受；明确继承A/B自动矩阵，不冒充全部重跑 |
| 2026-10-10 M4-B | [最终修复后QA](../../../build/qa-m4-wp03b/qa-fix-01/qa-report.md) | 实例/slot编辑、真正Clear/Load、同会话GUI和像素；更早拒绝/阻塞历史不改写 |
| 2026-10-08 M4-A | [QA](../../../build/qa-m4-wp03a/qa-report.md) | 路径/GBuffer、配置/CLI/UI及本包矩阵 |
| M4-WP02 / WP01 | [WP02](../../../build/qa-m4-wp02/qa-report.md)、[WP01](../../../build/qa-m4-wp01/qa-report.md) | HDR/深度/格式/材质与早期Deferred局部范围，各日期计数不充当当前总数 |
| 2026-10-21 双帧子范围 | [QA](../../../build/qa-m5-dual-frame/qa-report.md) | 1/2帧、真实双槽、38项Debug及独立像素/GUI等；不是M5整体完成 |
| 2026-10-21 CornellBox | [QA](../../../build/qa-cornell-box/qa-report.md) | 两配置core142、Debug定向7/7、真实GPU像素/GUI；未跑当时完整33项 |
| 2026-10-24 VolumetricIntegration | [QA](../../../build/qa-volumetric-integration/qa-report.md) | 两配置core145/options4，Debug定向8/8、Release3/3零skip；每配置11场景/36提交 |

Mclaren人工证据按原记录区分：独立持续Orbit/释放/UI-origin/W/S；WASDQE、右键持续转向、UI文本capture/失焦含用户明确 USER-SELFTESTED 与重复 SKIPPED。专项held/diagonal/startup/mode/reset/epoch stress、exact physical rates等 NOT RUN，不用CPU数学测试冒充全部实机输入。

Release CTest的77/skip不是GPU通过，真实Release GPU退出0也不等于捕获Validation。Validation error0只覆盖实际启用并成功捕获的运行；旧duplicate EOS loader warning与应用模型warning区分。OOM/device-loss/低能力/强制live-format/soak/RenderDoc等未运行项按各报告保留，正常候选失败回退不能代替物理故障注入。

体积积分的独立无噪声原作CPU参考为5场景/890像素，baseline645120像素PPM编码字节误差0；来源是Graph线性half与CPU预览，不是交换链截图。原站/原纹理403，原站实时逐像素、强制物理sRGB等 NOT RUN，细节/证据目录在zip中的旧usage原文和正式QA。

### 2026-10-24格式调整补验

用户的main.cpp等效括号/排版调整保留，设计事实不变。首次补验冻结失效记录保留，由[第二次独立QA ACCEPTED](../../../build/qa-volumetric-integration/post-format-2-qa-report.md)闭合；[新17文件冻结清单](../../../build/volumetric-integration-dev/post-format-2-frozen-files.json)中16份与原验收一致，仅main.cpp为格式版本（SHA256 AD29936FD42EB81DA76803B11BE141015DF8796D667589200C74F719B6CDF29A），见[补充交接](../../../build/volumetric-integration-dev/post-format-2-handoff.md)。

补验通过两配置增量构建、Debug显式SyncVal3提交/2槽/2draws6vertices、尾帧齐全、三个raw与原固定时间baseline逐字节一致；无帧数或0帧读回实际退出2，新冻结前后零失配。未重跑完整矩阵、旧例/GUI/独立原作参考，沿用原证据及其边界；旧17文件hash只描述原历史冻结。
