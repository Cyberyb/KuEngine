# 使用与回归说明

这里维护当前可执行的构建、运行和调试方法。设计见 [design](../design/README.md)，历史发布说明见 [archive](../structure/archive/README.md)。

- [Triangle](triangle-example.md)
- [Cube](cube-example.md)
- [Alpha3Pass](alpha3pass-example.md)
- [Mclaren](mclaren-example.md)
- [ForwardReuse / 双路径线性HDR验证](forward-reuse-example.md)
- [GraphResourceProbe](graph-resource-probe-example.md)
- [CornellBox / 程序化延迟 PBR](cornell-box-example.md)
- [VolumetricIntegration / SebH XlBSRz 体积光积分](volumetric-integration-example.md)
- [开发渲染特性：Graphics / Compute / native 接入](developing-render-features.md)
- [当前回归检查](regression-checks.md)
- [Shader 源码调试](shader-debugging.md)

两种构建入口输出不同：
- CMake preset：build/cmake-tools/vs2022-vcpkg/bin/Debug；
- 既有示例 run_*.bat：build/bin/Debug；VolumetricIntegration 新脚本使用 preset 目录，可用 KUENGINE_BUILD_DIR 覆盖。

手动启动前进入对应 exe 所在目录，保证 shaders/resources 的相对路径正确。

2026-10-11 C最终QA ACCEPTED + DOCUMENTED，REQ-01～13首轮范围已验收，WP03/M4已由Lead最终审计闭环（DONE），M5未开始。Mclaren操作与现有Graph特性接入见上列入口；永久29states/16edits/layout×2与20真实Release GPU继承见回归，77skip不是GPU通过。独立相机观察、USER-SELFTESTED/重复SKIPPED和新专项stress NOT RUN保持区分。

2026-10-21 当前七个示例；CornellBox / REQ-15 独立 QA ACCEPTED，默认 Deferred，运行/读回与本包精确验收范围见上列入口。

2026-10-24 当前八个示例；VolumetricIntegration / REQ-16 独立 QA ACCEPTED + DOCUMENTED，固定时间、积分对比、读回和精确证据边界见上列入口。M5-WP01 仍实施中，独立验收不随本包完成。
