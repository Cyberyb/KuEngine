#include "Log.h"

#include <spdlog/sinks/stdout_color_sinks.h>

namespace ku::log {

void init() {
    auto console = spdlog::stdout_color_mt("console");
    spdlog::set_default_logger(console);
    
    // 设置日志格式
    spdlog::set_pattern("[%H:%M:%S] [%^%l%$] %v");

#ifdef KU_DEBUG_BUILD
    spdlog::set_level(spdlog::level::debug);
#else
    spdlog::set_level(spdlog::level::info);
#endif
}

} // namespace ku::log
