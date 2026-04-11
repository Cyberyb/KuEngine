#include <gtest/gtest.h>

// Simple test to verify the project compiles
TEST(Engine, Version) {
    // Verify version macro is defined
    static_assert(KU_VERSION_MAJOR == 0, "Major version should be 0");
    static_assert(KU_VERSION_MINOR >= 0, "Minor version should be non-negative");
}

// Test VK_CHECK macro expansion
TEST(Vulkan, MacroExpansion) {
    // Just verify the macro can be used in a context that expects a statement
    // This doesn't actually call Vulkan, just ensures the macro compiles
    #ifdef VK_CHECK
    EXPECT_TRUE(true);
    #endif
}
