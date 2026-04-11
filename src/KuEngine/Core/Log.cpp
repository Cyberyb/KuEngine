#include "Log.h"

#include <spdlog/sinks/stdout_color_sinks.h>

namespace ku::log {

void init()
{
    auto logger = spdlog::get("KuEngine");
    if (!logger) {
        logger = spdlog::stdout_color_mt("KuEngine");
    }

    spdlog::set_default_logger(logger);
#if KU_DEBUG_BUILD
    spdlog::set_level(spdlog::level::debug);
#else
    spdlog::set_level(spdlog::level::info);
#endif
    spdlog::set_pattern("[%T] [%n] [%^%l%$] %v");
}

} // namespace ku::log
