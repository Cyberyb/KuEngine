// KuEngine 日志模块：封装日志系统的初始化、关闭与分级输出接口。
#pragma once

#include <spdlog/spdlog.h>

namespace ku::log {

void init();

} // namespace ku::log

#define KU_TRACE(...) ::spdlog::trace(__VA_ARGS__)
#define KU_DEBUG(...) ::spdlog::debug(__VA_ARGS__)
#define KU_INFO(...)  ::spdlog::info(__VA_ARGS__)
#define KU_WARN(...)  ::spdlog::warn(__VA_ARGS__)
#define KU_ERROR(...) ::spdlog::error(__VA_ARGS__)
#define KU_CRITICAL(...) ::spdlog::critical(__VA_ARGS__)
