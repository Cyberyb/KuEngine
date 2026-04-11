// KuEngine core tests
#include <gtest/gtest.h>
#include <KuEngine/Core/Engine.h>

TEST(KuEngineTest, VersionMacro)
{
    EXPECT_STREQ(KU_VERSION, "0.1.0");
}
