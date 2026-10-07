#include "ApplicationRunner.h"
#include "RuntimeError.h"

#include "../RHI/RHIInstance.h"
#include "../RHI/RHIDevice.h"
#include "../RHI/VulkanValidation.h"

#include <charconv>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace ku {
namespace {

enum class ApplicationPhase {
    Initialization,
    Running,
};

uint64_t parseUnsigned(std::string_view value, std::string_view option)
{
    uint64_t parsed = 0;
    const char* begin = value.data();
    const char* end = begin + value.size();
    const auto result = std::from_chars(begin, end, parsed);
    if (value.empty() || result.ec != std::errc{} || result.ptr != end) {
        throw std::invalid_argument(
            std::string(option) + " requires a non-negative integer");
    }
    return parsed;
}

uint32_t parseDimension(std::string_view value, std::string_view option)
{
    const uint64_t parsed = parseUnsigned(value, option);
    if (parsed == 0 || parsed > std::numeric_limits<uint32_t>::max()) {
        throw std::invalid_argument(
            std::string(option) + " requires a positive 32-bit dimension");
    }
    return static_cast<uint32_t>(parsed);
}

void printValidationFailure(const ValidationMessageCounts& counts)
{
    std::cerr
        << "KUENGINE_SMOKE_FAIL reason=validation warnings=" << counts.warnings
        << " errors=" << counts.errors << '\n';
}

void printCompletedStatistics(
    const CompletedFrameStatistics& completed,
    const CommandListStatistics& expected)
{
    std::cout
        << "KUENGINE_COMPLETED_STATS frame=" << completed.submittedFrame
        << " draws=" << completed.commands.drawCalls
        << " vertices=" << completed.commands.submittedVertices
        << " cpu_ms=" << completed.cpuTimeMilliseconds
        << " gpu_status=" << toString(completed.gpuTime.status);
    if (completed.gpuTime.status == GpuTimeStatus::Available) {
        std::cout << " gpu_ms=" << completed.gpuTime.milliseconds;
    }
    std::cout
        << " expected_draws=" << expected.drawCalls
        << " expected_vertices=" << expected.submittedVertices
        << " status=matched\n";
}

void printViewerLayout(
    std::string_view sample,
    const ViewerLayout& layout)
{
    std::cout
        << "KUENGINE_VIEWER_LAYOUT sample=" << sample
        << " logical=" << layout.windowLogical.width
        << 'x' << layout.windowLogical.height
        << " framebuffer=" << layout.framebufferExtent.width
        << 'x' << layout.framebufferExtent.height
        << " scene_logical=" << layout.sceneLogical.width
        << 'x' << layout.sceneLogical.height
        << " scene_pixels=" << layout.sceneFramebuffer.width
        << 'x' << layout.sceneFramebuffer.height
        << " aspect=" << layout.sceneAspect
        << " sidebar_reserved=" << (layout.sidebarReserved ? 1 : 0)
        << " overlay_fallback=" << (layout.sidebarOverlayFallback ? 1 : 0)
        << '\n';
}

bool validViewerLayout(
    const ViewerLayout& layout,
    bool expectExpanded) noexcept
{
    const bool baseValid = layout.sceneRenderable()
        && static_cast<uint64_t>(layout.sceneFramebuffer.x)
                + layout.sceneFramebuffer.width
            <= layout.framebufferExtent.width
        && static_cast<uint64_t>(layout.sceneFramebuffer.y)
                + layout.sceneFramebuffer.height
            <= layout.framebufferExtent.height
        && std::isfinite(layout.sceneAspect)
        && layout.sceneAspect > 0.0f;
    if (!baseValid) {
        return false;
    }

    if (expectExpanded && !layout.sidebarOverlayFallback) {
        return layout.sidebarReserved
            && layout.sceneFramebuffer.width < layout.framebufferExtent.width;
    }
    if (!expectExpanded) {
        return !layout.sidebarReserved
            && layout.sceneFramebuffer.width == layout.framebufferExtent.width;
    }
    return true;
}

} // namespace

ApplicationRunOptions parseApplicationRunOptions(
    std::span<const std::string_view> arguments)
{
    ApplicationRunOptions options{};

    for (size_t i = 0; i < arguments.size(); ++i) {
        const std::string_view argument = arguments[i];
        if (argument == "--smoke-frames") {
            if (++i >= arguments.size()) {
                throw std::invalid_argument("--smoke-frames requires a value");
            }
            options.engine.submittedFrameLimit =
                parseUnsigned(arguments[i], "--smoke-frames");
            options.smokeArgumentsPresent = true;
        } else if (argument == "--smoke-require-validation") {
            options.requireValidation = true;
            options.smokeArgumentsPresent = true;
        } else if (argument == "--smoke-inject-validation-error") {
            options.injectValidationError = true;
            options.smokeArgumentsPresent = true;
        } else if (argument == "--smoke-recompile-after-setup") {
            options.recompileAfterSetup = true;
            options.smokeArgumentsPresent = true;
        } else if (argument == "--smoke-sidebar-collapsed") {
            options.engine.sidebarExpanded = false;
            options.smokeArgumentsPresent = true;
        } else if (argument == "--smoke-resize-after") {
            if (i + 3 >= arguments.size()) {
                throw std::invalid_argument(
                    "--smoke-resize-after requires FRAME WIDTH HEIGHT");
            }
            EngineResizeRequest resize{};
            resize.afterSubmittedFrame =
                parseUnsigned(arguments[++i], "--smoke-resize-after FRAME");
            resize.width = parseDimension(arguments[++i], "--smoke-resize-after WIDTH");
            resize.height = parseDimension(arguments[++i], "--smoke-resize-after HEIGHT");
            options.engine.resize = resize;
            options.smokeArgumentsPresent = true;
        } else {
            throw std::invalid_argument("Unknown KuEngine application argument: " + std::string(argument));
        }
    }

    if (options.injectValidationError
        && (!options.requireValidation || options.engine.submittedFrameLimit == 0)) {
        throw std::invalid_argument(
            "Validation error injection requires --smoke-require-validation and a positive frame limit");
    }
    if (options.engine.resize) {
        const EngineResizeRequest& resize = *options.engine.resize;
        if (resize.afterSubmittedFrame == 0) {
            throw std::invalid_argument("Smoke resize frame must be greater than zero");
        }
        if (resize.afterSubmittedFrame > std::numeric_limits<uint64_t>::max() - 2
            || options.engine.submittedFrameLimit == 0
            || options.engine.submittedFrameLimit < resize.afterSubmittedFrame + 2) {
            throw std::invalid_argument(
                "Smoke resize requires a frame limit at least two frames after the resize point");
        }
    }

    return options;
}

int runApplication(
    int argc,
    char* argv[],
    EngineConfig config,
    ConfigureApplication configure)
{
    ApplicationRunOptions options{};
    try {
        std::vector<std::string_view> arguments;
        arguments.reserve(argc > 1 ? static_cast<size_t>(argc - 1) : 0);
        for (int i = 1; i < argc; ++i) {
            arguments.emplace_back(argv[i]);
        }
        options = parseApplicationRunOptions(arguments);
    } catch (const std::invalid_argument& error) {
        std::cerr << "KUENGINE_ARGUMENT_ERROR: " << error.what() << '\n';
        return static_cast<int>(ApplicationExitCode::invalidArguments);
    }

    auto validationMessages = std::make_shared<ValidationMessageTracker>();
    EngineRunResult result{};
    bool validationUnavailable = false;
    std::string validationUnavailableReason;
    ApplicationPhase phase = ApplicationPhase::Initialization;

    try {
        {
            Engine engine(std::move(config), validationMessages);
            if (options.requireValidation
                && !engine.instance().validationMessageCaptureEnabled()) {
                validationUnavailable = true;
                if (!engine.instance().validationRequested()) {
                    validationUnavailableReason = "validation is not requested in this build";
                } else if (!engine.instance().validationEnabled()) {
                    validationUnavailableReason = "VK_LAYER_KHRONOS_validation is unavailable";
                } else {
                    validationUnavailableReason = "VK_EXT_debug_utils message capture is unavailable";
                }
            } else {
                if (options.injectValidationError
                    && !engine.instance().submitValidationTestErrorForSmokeTest()) {
                    throw std::runtime_error(
                        "Unable to submit the controlled Validation smoke message");
                }
                configure(engine);
                engine.compile();
                if (options.recompileAfterSetup) {
                    engine.device().waitIdle();
                    engine.compile();
                    std::cout << "KUENGINE_SMOKE_RECOMPILE_OK\n";
                }
                phase = ApplicationPhase::Running;
                result = engine.run(options.engine);
            }
        }
    } catch (const RuntimeUnavailableError& error) {
        const ValidationMessageCounts counts = validationMessages->counts();
        if (counts.errors > 0) {
            printValidationFailure(counts);
            return static_cast<int>(ApplicationExitCode::validationFailure);
        }
        if (options.smokeArgumentsPresent) {
            std::cout << "KUENGINE_SMOKE_SKIP reason=" << error.what() << '\n';
            return static_cast<int>(ApplicationExitCode::skipped);
        }
        std::cerr << "Fatal error: " << error.what() << '\n';
        return static_cast<int>(ApplicationExitCode::runtimeFailure);
    } catch (const std::exception& error) {
        const ValidationMessageCounts counts = validationMessages->counts();
        if (counts.errors > 0) {
            printValidationFailure(counts);
            return static_cast<int>(ApplicationExitCode::validationFailure);
        }
        if (options.smokeArgumentsPresent) {
            std::cerr
                << "KUENGINE_SMOKE_FAIL reason="
                << (phase == ApplicationPhase::Initialization
                    ? "initialization"
                    : "runtime")
                << " message=" << error.what() << '\n';
        } else {
            std::cerr << "Fatal error: " << error.what() << '\n';
        }
        return static_cast<int>(ApplicationExitCode::runtimeFailure);
    }

    const ValidationMessageCounts counts = validationMessages->counts();
    if (counts.errors > 0) {
        printValidationFailure(counts);
        return static_cast<int>(ApplicationExitCode::validationFailure);
    }
    if (validationUnavailable) {
        std::cout << "KUENGINE_SMOKE_SKIP reason=" << validationUnavailableReason << '\n';
        return static_cast<int>(ApplicationExitCode::skipped);
    }
    if (options.engine.submittedFrameLimit > 0 && !result.reachedFrameLimit) {
        std::cerr
            << "KUENGINE_SMOKE_FAIL reason=frame_limit_not_reached submitted_frames="
            << result.submittedFrames << '\n';
        return static_cast<int>(ApplicationExitCode::runtimeFailure);
    }
    if (options.engine.resize && !result.resizeCompleted) {
        std::cerr << "KUENGINE_SMOKE_FAIL reason=resize_not_completed\n";
        return static_cast<int>(ApplicationExitCode::runtimeFailure);
    }
    if (options.engine.submittedFrameLimit > 0) {
        if (!result.firstSubmittedViewerLayout.has_value()
            || !result.finalSubmittedViewerLayout.has_value()) {
            std::cerr << "KUENGINE_SMOKE_FAIL reason=viewer_layout_unavailable\n";
            return static_cast<int>(ApplicationExitCode::runtimeFailure);
        }

        const bool expectExpanded =
            options.engine.sidebarExpanded.value_or(true);
        if (!validViewerLayout(
                *result.firstSubmittedViewerLayout,
                expectExpanded)
            || !validViewerLayout(
                *result.finalSubmittedViewerLayout,
                expectExpanded)) {
            std::cerr << "KUENGINE_SMOKE_FAIL reason=viewer_layout_invalid\n";
            return static_cast<int>(ApplicationExitCode::runtimeFailure);
        }
        const ViewerLayout& first = *result.firstSubmittedViewerLayout;
        const ViewerLayout& final = *result.finalSubmittedViewerLayout;
        const bool resizeRequestsLogicalChange = options.engine.resize
            && (std::abs(
                    first.windowLogical.width
                    - static_cast<double>(options.engine.resize->width)) > 0.5
                || std::abs(
                    first.windowLogical.height
                    - static_cast<double>(options.engine.resize->height)) > 0.5);
        const bool logicalResizeObserved =
            std::abs(first.windowLogical.width - final.windowLogical.width) > 0.5
            || std::abs(first.windowLogical.height - final.windowLogical.height) > 0.5;
        if (resizeRequestsLogicalChange && !logicalResizeObserved) {
            std::cerr << "KUENGINE_SMOKE_FAIL reason=viewer_layout_resize_not_observed\n";
            return static_cast<int>(ApplicationExitCode::runtimeFailure);
        }
        printViewerLayout("first", first);
        printViewerLayout("final", final);

        if (!result.completedStatistics.has_value()) {
            std::cerr << "KUENGINE_SMOKE_FAIL reason=completed_stats_unavailable\n";
            return static_cast<int>(ApplicationExitCode::runtimeFailure);
        }
        if (!result.expectedStatistics.has_value()) {
            std::cerr << "KUENGINE_SMOKE_FAIL reason=expected_stats_unavailable\n";
            return static_cast<int>(ApplicationExitCode::runtimeFailure);
        }

        const CommandListStatistics& actual =
            result.completedStatistics->commands;
        const CommandListStatistics& expected =
            *result.expectedStatistics;
        if (!commandStatisticsEqual(actual, expected)) {
            std::cerr
                << "KUENGINE_SMOKE_FAIL reason=stats_mismatch"
                << " actual_draws=" << actual.drawCalls
                << " actual_vertices=" << actual.submittedVertices
                << " expected_draws=" << expected.drawCalls
                << " expected_vertices=" << expected.submittedVertices
                << '\n';
            return static_cast<int>(ApplicationExitCode::runtimeFailure);
        }
        printCompletedStatistics(*result.completedStatistics, expected);
    }
    if (options.smokeArgumentsPresent) {
        std::cout
            << "KUENGINE_SMOKE_PASS submitted_frames=" << result.submittedFrames
            << " warnings=" << counts.warnings
            << " errors=" << counts.errors << '\n';
    }
    return static_cast<int>(ApplicationExitCode::success);
}

} // namespace ku
