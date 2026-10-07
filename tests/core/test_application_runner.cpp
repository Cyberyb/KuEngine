#include <gtest/gtest.h>

#include "KuEngine/Core/ApplicationRunner.h"

#include <array>
#include <string_view>

namespace {

TEST(ApplicationRunner, DefaultsToUnlimitedInteractiveRun)
{
    const ku::ApplicationRunOptions options =
        ku::parseApplicationRunOptions({});

    EXPECT_FALSE(options.smokeArgumentsPresent);
    EXPECT_EQ(options.engine.submittedFrameLimit, 0u);
    EXPECT_FALSE(options.engine.resize.has_value());
    EXPECT_FALSE(options.engine.sidebarExpanded.has_value());
    EXPECT_FALSE(options.requireValidation);
    EXPECT_FALSE(options.injectValidationError);
    EXPECT_FALSE(options.recompileAfterSetup);
}

TEST(ApplicationRunner, ParsesCollapsedSidebarSmokeMode)
{
    constexpr std::array<std::string_view, 3> arguments{
        "--smoke-frames", "3", "--smoke-sidebar-collapsed",
    };
    const ku::ApplicationRunOptions options =
        ku::parseApplicationRunOptions(arguments);

    ASSERT_TRUE(options.engine.sidebarExpanded.has_value());
    EXPECT_FALSE(*options.engine.sidebarExpanded);
    EXPECT_TRUE(options.smokeArgumentsPresent);
}

TEST(ApplicationRunner, ParsesBoundedValidationResizeRun)
{
    constexpr std::array<std::string_view, 8> arguments{
        "--smoke-frames", "6",
        "--smoke-require-validation",
        "--smoke-resize-after", "2", "960", "540",
        "--smoke-inject-validation-error",
    };

    const ku::ApplicationRunOptions options =
        ku::parseApplicationRunOptions(arguments);

    EXPECT_TRUE(options.smokeArgumentsPresent);
    EXPECT_EQ(options.engine.submittedFrameLimit, 6u);
    ASSERT_TRUE(options.engine.resize.has_value());
    EXPECT_EQ(options.engine.resize->afterSubmittedFrame, 2u);
    EXPECT_EQ(options.engine.resize->width, 960u);
    EXPECT_EQ(options.engine.resize->height, 540u);
    EXPECT_TRUE(options.requireValidation);
    EXPECT_TRUE(options.injectValidationError);
}

TEST(ApplicationRunner, ZeroFrameLimitRetainsUnlimitedMeaning)
{
    constexpr std::array<std::string_view, 2> arguments{
        "--smoke-frames", "0",
    };

    const ku::ApplicationRunOptions options =
        ku::parseApplicationRunOptions(arguments);
    EXPECT_TRUE(options.smokeArgumentsPresent);
    EXPECT_EQ(options.engine.submittedFrameLimit, 0u);
}

TEST(ApplicationRunner, ParsesControlledPipelineRecompile)
{
    constexpr std::array<std::string_view, 3> arguments{
        "--smoke-frames", "3", "--smoke-recompile-after-setup",
    };

    const ku::ApplicationRunOptions options =
        ku::parseApplicationRunOptions(arguments);
    EXPECT_TRUE(options.recompileAfterSetup);
    EXPECT_TRUE(options.smokeArgumentsPresent);
    EXPECT_EQ(options.engine.submittedFrameLimit, 3u);
}

TEST(ApplicationRunner, RejectsUnsafeInjectionAndInvalidResize)
{
    constexpr std::array<std::string_view, 3> injectionWithoutValidation{
        "--smoke-frames", "3", "--smoke-inject-validation-error",
    };
    EXPECT_THROW(
        (void)ku::parseApplicationRunOptions(injectionWithoutValidation),
        std::invalid_argument);

    constexpr std::array<std::string_view, 6> resizeWithoutPostFrames{
        "--smoke-frames", "3",
        "--smoke-resize-after", "2", "960", "540",
    };
    EXPECT_THROW(
        (void)ku::parseApplicationRunOptions(resizeWithoutPostFrames),
        std::invalid_argument);

    constexpr std::array<std::string_view, 1> unknown{"--unknown"};
    EXPECT_THROW(
        (void)ku::parseApplicationRunOptions(unknown),
        std::invalid_argument);
}

} // namespace
