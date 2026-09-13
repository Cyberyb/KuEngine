#include <gtest/gtest.h>

#include "KuEngine/Core/Log.h"
#include "KuEngine/RHI/RHICommon.h"

namespace {

TEST(LogConfiguration, MatchesConsumerBuildConfiguration)
{
    const auto expected = KU_DEBUG_BUILD != 0
        ? spdlog::level::debug
        : spdlog::level::info;

    EXPECT_EQ(ku::log::configuredLevel(), expected);

    ku::log::init();
    EXPECT_EQ(spdlog::get_level(), expected);
}

} // namespace
