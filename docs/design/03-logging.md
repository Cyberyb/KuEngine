# 日志与诊断设计

核对日期：2026-09-12。

源码：[Log.h](../../src/KuEngine/Core/Log.h)、[Log.cpp](../../src/KuEngine/Core/Log.cpp)、[RHICommon.h](../../src/KuEngine/RHI/RHICommon.h)、[RHIInstance.cpp](../../src/KuEngine/RHI/RHIInstance.cpp)。

## 应用日志

ku::log::init() 查找或创建名为 KuEngine 的 stdout_color_mt logger，将其设为 spdlog 默认 logger。格式为时间、logger 名称、级别和消息。

KU_TRACE / KU_DEBUG / KU_INFO / KU_WARN / KU_ERROR / KU_CRITICAL 转发到 spdlog 的对应接口。当前没有文件 Sink、异步日志、ImGui 日志控制台或日志导出模块。

Log.cpp 按 KU_DEBUG_BUILD 选择 debug/info 级别。该宏定义在 RHICommon.h，而 Log.cpp 没有直接包含它；当前 CMake 也未统一定义该宏，因此不能仅凭 Debug 配置保证这里会开启 debug 日志。此处记录现有实现边界。

## Vulkan 错误

VK_CHECK 对非 VK_SUCCESS 返回值抛出 runtime_error，包含 VkResult 名称、文件行号和原始表达式。可恢复状态必须由调用点先分支处理，例如交换链的 OUT_OF_DATE/SUBOPTIMAL，以及 Query 读取的 NOT_READY。

不是所有 Vulkan 调用都经过 VK_CHECK；例如 SyncManager 的部分 Fence 调用直接使用返回值未检查。UIOverlay 的 Backend 检查回调则通过 KU_ERROR 记录非成功结果。

## Validation 与诊断来源

RHIInstance 在 KU_DEBUG_BUILD 下请求 VK_LAYER_KHRONOS_validation。当前不枚举可用 Layer，也没有创建 VkDebugUtilsMessengerEXT 将验证消息统一接入 logger。启用 Layer 和拥有完整错误收集设施是两个不同的实现状态。

RenderPipeline 的声明、编译和执行校验会报告 Pass/资源问题，Graph Debug UI 展示编译计划与最近执行摘要。示例 main 捕获 std::exception 并输出 Fatal error。

具体问题的复现、根因和回归过程写入 [bugs](../bugs/README.md)；每日完成摘要写入 [logs](../logs/README.md)，不在本设计文件复制 Bug 模板或历史案例。
