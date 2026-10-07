// KuEngine 应用启动器：共享示例参数解析、有限帧运行与冒烟退出语义。
#pragma once

#include "Engine.h"

#include <functional>
#include <span>
#include <string_view>

namespace ku {

struct ApplicationRunOptions {
    EngineRunOptions engine;
    bool smokeArgumentsPresent = false;
    bool requireValidation = false;
    bool injectValidationError = false;
    bool recompileAfterSetup = false;
};

enum class ApplicationExitCode : int {
    success = 0,
    runtimeFailure = 1,
    invalidArguments = 2,
    validationFailure = 3,
    skipped = 77,
};

[[nodiscard]] ApplicationRunOptions parseApplicationRunOptions(
    std::span<const std::string_view> arguments);

using ConfigureApplication = std::function<void(Engine&)>;

[[nodiscard]] int runApplication(
    int argc,
    char* argv[],
    EngineConfig config,
    ConfigureApplication configure);

} // namespace ku
