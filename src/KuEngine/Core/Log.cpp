#include "Log.h"

#include <spdlog/sinks/stdout_color_sinks.h>

namespace ku::log {

spdlog::level::level_enum configuredLevel() noexcept
{
#if KU_DEBUG_BUILD
    return spdlog::level::debug;
#else
    return spdlog::level::info;
#endif
}

void init()
{
    auto logger = spdlog::get("KuEngine");
    if (!logger) {
        logger = spdlog::stdout_color_mt("KuEngine");
    }

    spdlog::set_default_logger(logger);
    spdlog::set_level(configuredLevel());
    spdlog::set_pattern("[%T] [%n] [%^%l%$] %v");
}

} // namespace ku::log
