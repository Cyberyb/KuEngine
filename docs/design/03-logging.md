# 日志与诊断设计

核对日期：2026-09-14。

源码：[Log.h](../../src/KuEngine/Core/Log.h)、[Log.cpp](../../src/KuEngine/Core/Log.cpp)、[RHICommon.h](../../src/KuEngine/RHI/RHICommon.h)、[RHIInstance.cpp](../../src/KuEngine/RHI/RHIInstance.cpp)、[VulkanValidation](../../src/KuEngine/RHI/VulkanValidation.h)。

## 应用日志

ku::log::init() 查找或创建名为 KuEngine 的 stdout_color_mt logger，将其设为 spdlog 默认 logger。格式为时间、logger 名称、级别和消息。

KU_TRACE / KU_DEBUG / KU_INFO / KU_WARN / KU_ERROR / KU_CRITICAL 转发到 spdlog 的对应接口。当前没有文件 Sink、异步日志、ImGui 日志控制台或日志导出模块。

库目标通过 CMake 以配置生成式定义 `KU_DEBUG_BUILD`：Debug 为 1，其余配置为 0；`RHICommon.h` 只在该定义未提供时以 `NDEBUG` 给出回退值。`Log.cpp` 通过 `configuredLevel()` 按此宏选择 debug/info 级别，测试覆盖该选择与初始化后的 spdlog 级别。

## Vulkan 错误

VK_CHECK 对非 VK_SUCCESS 返回值抛出 runtime_error，包含 VkResult 名称、文件行号和原始表达式。可恢复状态必须由调用点先分支处理，例如交换链的 OUT_OF_DATE/SUBOPTIMAL，以及 Query 读取的 NOT_READY。

不是所有 Vulkan 调用都经过 VK_CHECK；例如 SyncManager 的部分 Fence 调用直接使用返回值未检查。UIOverlay 的 Backend 检查回调则通过 KU_ERROR 记录非成功结果。

## Validation 与诊断来源

Debug 配置中的 `RHIInstance` 请求 `VK_LAYER_KHRONOS_validation`，并先枚举该 Layer 与 `VK_EXT_debug_utils`。缺少 Layer 时以警告说明并继续运行但禁用 Validation；Layer 存在而 Debug Utils 缺失时保持 Layer 启用、但 KuEngine 不捕获回调消息。两者都可用时，`VkDebugUtilsMessengerEXT` 覆盖 Instance 创建期间及其存活期间的 general、validation、performance 消息。

`VulkanValidationState` 将 callback 收到的 warning/error 写入原子的 `ValidationMessageTracker`，并将完整消息写入默认 spdlog logger：error、warning、info、verbose 分别映射到 `KU_ERROR`、`KU_WARN`、`KU_INFO`、`KU_TRACE`。error 额外带有 `KUENGINE_VALIDATION_ERROR` 标记。回调捕获日志异常且返回 `VK_FALSE`，不会从 Vulkan 回调跨越 C ABI 抛出异常。Tracker 可由 `ApplicationRunner` 在 Engine/Instance 销毁后读取；任何由该运行器完成的应用运行只要 error 非零，都会输出 `KUENGINE_SMOKE_FAIL reason=validation` 并返回退出码 3。`--smoke-require-validation` 只额外要求消息捕获能力可用，以让自动化结果可解释。

冒烟运行的稳定结果标记是 `KUENGINE_SMOKE_PASS`、`KUENGINE_SMOKE_FAIL` 与 `KUENGINE_SMOKE_SKIP`。`RuntimeUnavailableError`（例如 Vulkan 驱动、Surface、设备不可用）仅在提供任一 smoke 参数时映射为 skip/77；缺少 Validation Layer、Debug Utils 或 Release 构建中的 Validation 请求同样在 `--smoke-require-validation` 下映射为 skip/77。初始化/资源等业务错误仍为 fail/1，不能因运行在 smoke 模式而被归类为环境跳过。

RenderPipeline 的声明、编译和执行校验会报告 Pass/资源问题，Graph Debug UI 展示编译计划与最近执行摘要。示例 main 捕获 std::exception 并输出 Fatal error。

具体问题的复现、根因和回归过程写入 [bugs](../bugs/README.md)；每日完成摘要写入 [logs](../logs/README.md)，不在本设计文件复制 Bug 模板或历史案例。
