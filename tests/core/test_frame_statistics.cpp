#include <gtest/gtest.h>

#include <KuEngine/Core/FrameStatistics.h>

#include <cstdint>
#include <limits>

namespace {

TEST(FrameStatistics, DrawAndIndexedAccountingUsesInstances)
{
    ku::CommandStatisticsAccumulator statistics;
    statistics.recordDraw(3, 2);
    statistics.recordIndexedDraw(11, 4);
    statistics.recordDraw(99, 0);

    EXPECT_EQ(statistics.statistics().drawCalls, 3u);
    EXPECT_EQ(statistics.statistics().submittedVertices, 50u);
}

TEST(FrameStatistics, CommandTotalsSaturateInsteadOfWrapping)
{
    ku::CommandStatisticsAccumulator statistics;
    statistics.add(ku::CommandListStatistics{
        std::numeric_limits<uint64_t>::max(),
        std::numeric_limits<uint64_t>::max() - 2,
    });
    statistics.recordDraw(2, 2);

    EXPECT_EQ(
        statistics.statistics().drawCalls,
        std::numeric_limits<uint64_t>::max());
    EXPECT_EQ(
        statistics.statistics().submittedVertices,
        std::numeric_limits<uint64_t>::max());
}

TEST(FrameStatistics, ExpectedCommandStatisticsRequireBothFieldsToMatch)
{
    const ku::CommandListStatistics expected{3, 9};
    EXPECT_TRUE(ku::commandStatisticsEqual(expected, {3, 9}));
    EXPECT_FALSE(ku::commandStatisticsEqual(expected, {2, 9}));
    EXPECT_FALSE(ku::commandStatisticsEqual(expected, {3, 8}));
}

TEST(FrameStatistics, GpuTimingDistinguishesUnsupportedWaitingAndAvailable)
{
    const ku::GpuTimeSample unsupported = ku::resolveGpuTimeSample(
        false, false, 0, 0, 1.0f, 0);
    EXPECT_EQ(unsupported.status, ku::GpuTimeStatus::Unsupported);

    const ku::GpuTimeSample waiting = ku::resolveGpuTimeSample(
        true, false, 0, 0, 1.0f, 64);
    EXPECT_EQ(waiting.status, ku::GpuTimeStatus::Waiting);

    const ku::GpuTimeSample available = ku::resolveGpuTimeSample(
        true, true, 100, 350, 2.0f, 64);
    EXPECT_EQ(available.status, ku::GpuTimeStatus::Available);
    EXPECT_DOUBLE_EQ(available.milliseconds, 0.0005);
}

TEST(FrameStatistics, GpuTimingHandlesValidBitWrap)
{
    const ku::GpuTimeSample wrapped = ku::resolveGpuTimeSample(
        true,
        true,
        250,
        5,
        1'000.0f,
        8);

    ASSERT_EQ(wrapped.status, ku::GpuTimeStatus::Available);
    EXPECT_DOUBLE_EQ(wrapped.milliseconds, 0.011);
}

TEST(FrameStatistics, CompletedSnapshotsAdvanceOnlyForRecordedSubmissions)
{
    ku::CompletedFrameStatisticsTracker tracker;
    EXPECT_FALSE(tracker.completePendingFrame(
        {ku::GpuTimeStatus::Available, 0.25}));
    EXPECT_FALSE(tracker.completedFrame().has_value());

    ASSERT_TRUE(tracker.recordSubmittedFrame(
        0.75f,
        ku::CommandListStatistics{2, 18}));
    EXPECT_FALSE(tracker.recordSubmittedFrame(
        1.0f,
        ku::CommandListStatistics{9, 99}));
    ASSERT_TRUE(tracker.completePendingFrame(
        {ku::GpuTimeStatus::Available, 0.25}));

    ASSERT_TRUE(tracker.completedFrame().has_value());
    EXPECT_EQ(tracker.completedFrame()->submittedFrame, 1u);
    EXPECT_FLOAT_EQ(tracker.completedFrame()->cpuTimeMilliseconds, 0.75f);
    EXPECT_DOUBLE_EQ(tracker.completedFrame()->gpuTime.milliseconds, 0.25);
    EXPECT_EQ(tracker.completedFrame()->commands.drawCalls, 2u);
    EXPECT_EQ(tracker.completedFrame()->commands.submittedVertices, 18u);

    EXPECT_FALSE(tracker.completePendingFrame(
        {ku::GpuTimeStatus::Available, 99.0}));
    EXPECT_EQ(tracker.completedFrame()->submittedFrame, 1u);

    ASSERT_TRUE(tracker.recordSubmittedFrame(
        0.5f,
        ku::CommandListStatistics{1, 3}));
    ASSERT_TRUE(tracker.completePendingFrame(
        {ku::GpuTimeStatus::Waiting, 0.0}));
    EXPECT_EQ(tracker.completedFrame()->submittedFrame, 2u);
    EXPECT_EQ(
        tracker.completedFrame()->gpuTime.status,
        ku::GpuTimeStatus::Waiting);
}

} // namespace
