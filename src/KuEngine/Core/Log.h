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
