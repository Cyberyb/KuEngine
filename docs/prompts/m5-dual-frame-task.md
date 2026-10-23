# M5-WP01：双帧在途

日期：2026-10-21。用户明确要求仅实现 M5 多帧并行中的 2 帧，不实施其他 M5 要求。关联稳定需求 REQ-14，兼及 REQ-01/07/11 既有正确性。前置 M4 已闭环；现有 CornellBox 未提交改动属于用户工作区，须完整保留。

## 实施范围

- 默认示例与 Runtime 使用 2 帧在途；保留 1 帧对照，拒绝 0 或超过 2，不增加 3 帧。
- 每槽独立 CommandList/查询、CPU/GPU 完成统计、Frame/Draw UBO、可变 descriptor、Graph 内部 Image/Buffer 及物理状态；静态资产和不可变对象继续共享。不得只改配置或在正常帧使用全局 idle 伪装双帧。
- Acquire 与帧槽同步分离，render-finished semaphore 按交换链图像管理，安全复用与重建；处理 out-of-date、最小化、resize、recompile、退出及异常生命周期。
- 共享资产/descriptor/Pipeline 替换和动态容量变化先等待全部受影响提交，允许低频保守等待，不做通用 retire queue、异步上传或 batching。
- 所有现有示例（包括 CornellBox）和可选 GPU readback/smoke 消费者须正确使用实际完成提交；测试工具可选择单帧，但必须有真正双帧 GPU 输出和同步覆盖，不能全靠单帧测试。

非目标：其他 M5-WP02/03 功能、3 帧、逐 Pass profiler、预设、通用截图平台、批量上传、异步回收、多 Queue、Shader 画质功能、性能涨幅承诺。

## 写入与交接

Developer 为唯一代码/测试/构建写者，允许修改本任务必需的 Core/RHI/Render/UI、现有 examples/common 和各示例接入、tests/CMake 与相关测试。保留现有 CornellBox 改动，不提交、推送、删除用户文件或扩展范围。开始前读 AGENTS、docs/README、design/README 与对应模块设计。Developer 报 CODE_COMPLETE，列出命令/日志/风险/文档映射。

QA 期间冻结代码；QA 只写独立 build/qa-m5-dual-frame 下证据，不改源码/测试。通过后 Planner 唯一文档写者，同步 design 职责/接口/数据流/所有权/约束/图示、current-architecture、需求/路线/实施状态、usage、2026-10-21 日志（与 CornellBox 同日合并）。历史快照不重写，M5 整体不标完成。

## 必要验收

1. Debug/Release 构建、CPU 测试和双帧边界/槽位/统计契约；保留既有 CornellBox 几何覆盖。
2. 真实双帧提交与交替槽资源证据：首两次提交不等待上一帧槽、后续只复用已完成槽，至少连续多个轮转；证明不是每帧 device/queue idle。Timestamp/CPU/Draw 对齐同一实际提交，退出排空保留最新提交。
3. 实际 GPU/显式 Synchronization Validation：七示例双帧 smoke；Forward/Deferred、GBuffer、透明和 UI 资源；双帧 resize/recompile、最小化/恢复、模型与 HDR 成功/失败替换及 Clear/Load、路径切换/实例编辑。既有像素/readback 检查保持阈值，新增双帧稳定或交替参数输出证据。
4. 1 帧兼容回归，非法 0/3 配置；有限帧 1/2/奇数退出统计和 drain；没有 Validation error、提前销毁/覆盖/描述符改写或 Query 误归属。
5. 使用独立构建目录 build/m5-dual-frame-dev 和 build/qa-m5-dual-frame。基于本机 VS2022/vcpkg，KUENGINE_BUILD_EXAMPLES/TESTS/ENABLE_GPU_SMOKE_TESTS=ON；GPU 检查显式配置 VK_LAYER_ENABLES=VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT。每条实际命令、退出码、跳过/未运行与画面证据记录，构建或 CTest skip 不能替代 GPU 验收。

状态链：IMPLEMENTING → CODE_COMPLETE → QA ACCEPTED/REJECTED/VERIFICATION_BLOCKED → DOCUMENTED → Lead 核对；只有主 Agent 可报 DONE。
