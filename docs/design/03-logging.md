# 日志与调试规范

## 1. 日志系统

使用 spdlog 作为日志库。

### 日志级别定义

| 级别 | 使用场景 | 宏定义 |
|------|---------|--------|
| `trace` | 详细调试信息（Vulkan API 调用参数等） | `KU_TRACE` |
| `debug` | 开发调试信息 | `KU_DEBUG` |
| `info` | 一般信息（启动信息、状态变化） | `KU_INFO` |
| `warn` | 警告（性能问题、非致命错误） | `KU_WARN` |
| `error` | 错误（Vulkan 错误、异常） | `KU_ERROR` |
| `critical` | 严重错误（程序无法继续） | `KU_CRITICAL` |

### 宏定义

```cpp
// src/KuEngine/Core/Log.h
#pragma once

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

namespace ku {
namespace log {

void init();

} // namespace log
} // namespace ku

#define KU_TRACE(...) ::spdlog::trace(__VA_ARGS__)
#define KU_DEBUG(...) ::spdlog::debug(__VA_ARGS__)
#define KU_INFO(...)  ::spdlog::info(__VA_ARGS__)
#define KU_WARN(...)  ::spdlog::warn(__VA_ARGS__)
#define KU_ERROR(...) ::spdlog::error(__VA_ARGS__)
#define KU_CRITICAL(...) ::spdlog::critical(__VA_ARGS__)
```

### 使用示例

```cpp
KU_INFO("Initializing KuEngine v{}", KU_VERSION);
KU_DEBUG("Physical device: {}", deviceName);
KU_WARN("Validation layer enabled, performance may be reduced");
KU_ERROR("Failed to create swap chain: {}", to_string(result));
```

---

## 2. Vulkan 错误检查

### VK_CHECK 宏

```cpp
// src/KuEngine/RHI/RHICommon.h
#pragma once

#include <vulkan/vulkan.h>
#include <stdexcept>
#include <string>

constexpr const char* to_string(VkResult result) {
    switch (result) {
        case VK_SUCCESS: return "VK_SUCCESS";
        case VK_NOT_READY: return "VK_NOT_READY";
        case VK_TIMEOUT: return "VK_TIMEOUT";
        case VK_EVENT_SET: return "VK_EVENT_SET";
        case VK_EVENT_RESET: return "VK_EVENT_RESET";
        case VK_INCOMPLETE: return "VK_INCOMPLETE";
        case VK_ERROR_OUT_OF_HOST_MEMORY: return "VK_ERROR_OUT_OF_HOST_MEMORY";
        case VK_ERROR_OUT_OF_DEVICE_MEMORY: return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
        case VK_ERROR_INITIALIZATION_FAILED: return "VK_ERROR_INITIALIZATION_FAILED";
        case VK_ERROR_DEVICE_LOST: return "VK_ERROR_DEVICE_LOST";
        case VK_ERROR_MEMORY_MAP_FAILED: return "VK_ERROR_MEMORY_MAP_FAILED";
        case VK_ERROR_LAYER_NOT_PRESENT: return "VK_ERROR_LAYER_NOT_PRESENT";
        case VK_ERROR_EXTENSION_NOT_PRESENT: return "VK_ERROR_EXTENSION_NOT_PRESENT";
        case VK_ERROR_FEATURE_NOT_PRESENT: return "VK_ERROR_FEATURE_NOT_PRESENT";
        case VK_ERROR_INCOMPATIBLE_DRIVER: return "VK_ERROR_INCOMPATIBLE_DRIVER";
        case VK_ERROR_TOO_MANY_OBJECTS: return "VK_ERROR_TOO_MANY_OBJECTS";
        case VK_ERROR_FORMAT_NOT_SUPPORTED: return "VK_ERROR_FORMAT_NOT_SUPPORTED";
        case VK_ERROR_OUT_OF_DATE_KHR: return "VK_ERROR_OUT_OF_DATE_KHR";
        case VK_ERROR_SURFACE_LOST_KHR: return "VK_ERROR_SURFACE_LOST_KHR";
        case VK_SUBOPTIMAL_KHR: return "VK_SUBOPTIMAL_KHR";
        default: return "UNKNOWN_VK_RESULT";
    }
}

#define VK_CHECK(expr)                                                           \
    do {                                                                         \
        VkResult _result = (expr);                                               \
        if (_result != VK_SUCCESS) {                                             \
            throw std::runtime_error(                                            \
                std::string("Vulkan error: ") + to_string(_result) +             \
                " at " + __FILE__ + ":" + std::to_string(__LINE__) +            \
                "\nExpression: " + #expr                                          \
            );                                                                   \
        }                                                                        \
    } while (false)
```

---

## 3. Bug 追踪规范

### Bug 报告模板

在 `docs/bugs/` 下创建 `YYYY-MM-DD-issue-N.md`：

```markdown
# Bug Report: [简短描述]

**日期**: YYYY-MM-DD  
**严重程度**: Critical / High / Medium / Low  
**状态**: Open / In Progress / Resolved

## 复现步骤

1. 
2. 
3. 

## 预期行为

描述预期应该发生什么。

## 实际行为

描述实际发生了什么。

## 环境

- GPU: 
- Driver Version: 
- OS: Windows 11
- Vulkan SDK: 1.3.xxx

## 堆栈跟踪 / 错误信息

```
[粘贴错误信息]
```

## 分析

[分析可能的原因]

## 修复方案

[描述如何修复]
```

### 示例

```markdown
# Bug Report: SwapChain 重建时崩溃

**日期**: 2026-04-11  
**严重程度**: High  
**状态**: Resolved

## 复现步骤

1. 启动程序
2. 将窗口拖动到屏幕边缘触发 resize
3. 快速连续多次 resize

## 预期行为

窗口平滑 resize，不崩溃。

## 实际行为

程序崩溃，vkAcquireNextImageKHR 返回 VK_ERROR_OUT_OF_DATE_KHR 
后未正确处理。

## 修复方案

在 `SwapChain::acquireNextImage` 中添加重试逻辑，
当返回 `VK_ERROR_OUT_OF_DATE_KHR` 时自动调用 `recreate()` 并重试。
```

---

## 4. 调试工具集成

### RenderDoc

在 Debug 构建中自动注入 RenderDoc hook：

```cpp
// 在 Engine 初始化时调用
void initRenderDoc() {
#ifdef KU_DEBUG
    if (auto loader = RENDERDOC_GetAPI eLoaderFunc(RENDERDOC_API_1_1_2);
        loader(RENDERDOC_UUID_1_1_2, (void**)&rdoc) == 1) {
        rdoc->SetCaptureSavePath(savePath);
        KU_INFO("RenderDoc hook initialized");
    }
#endif
}
```

### Vulkan Configurator

打印关键 Vulkan 配置信息：

```cpp
void printVulkanInfo(const RHIDevice& device) {
    auto& props = device.physicalDeviceProperties();
    
    KU_INFO("=== Vulkan Info ===");
    KU_INFO("Driver Version: {}", props.driverVersion);
    KU_INFO("Vulkan Version: {}.{}.{}",
        VK_VERSION_MAJOR(props.apiVersion),
        VK_VERSION_MINOR(props.apiVersion),
        VK_VERSION_PATCH(props.apiVersion));
    KU_INFO("Device: {}", props.deviceName);
    KU_INFO("Device Type: {}", 
        [props.deviceType](auto types) {
            switch (props.deviceType) {
                case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU: return "Discrete";
                case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: return "Integrated";
                case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU: return "Virtual";
                default: return "Other";
            }
        });
}
```
