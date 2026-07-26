#include <gtest/gtest.h>

#include <limits>
#include <stdexcept>

#include <KuEngine/Render/PBRCommon.h>

TEST(PBRCommonTest, AlignsUniformStrideToDeviceRequirement)
{
    EXPECT_EQ(ku::alignedUniformBufferStride(sizeof(ku::PBRFrameUniforms), 256), 256u);
    EXPECT_EQ(ku::alignedUniformBufferStride(256, 256), 256u);
    EXPECT_EQ(ku::alignedUniformBufferStride(257, 256), 512u);
}

TEST(PBRCommonTest, KeepsStrideWhenAlignmentIsNotRequired)
{
    EXPECT_EQ(ku::alignedUniformBufferStride(192, 0), 192u);
    EXPECT_EQ(ku::alignedUniformBufferStride(192, 1), 192u);
}

TEST(PBRCommonTest, DetectsUniformStrideOverflow)
{
    constexpr VkDeviceSize maximum = std::numeric_limits<VkDeviceSize>::max();
    EXPECT_THROW(
        (void)ku::alignedUniformBufferStride(maximum, 256),
        std::overflow_error);
}
