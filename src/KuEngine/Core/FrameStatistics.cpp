#include "FrameStatistics.h"

#include <limits>

namespace ku {
namespace {

uint64_t saturatingAdd(uint64_t left, uint64_t right) noexcept
{
    constexpr uint64_t maximum = std::numeric_limits<uint64_t>::max();
    return right > maximum - left ? maximum : left + right;
}

} // namespace

bool commandStatisticsEqual(
    const CommandListStatistics& left,
    const CommandListStatistics& right) noexcept
{
    return left.drawCalls == right.drawCalls
        && left.submittedVertices == right.submittedVertices;
}

void CommandStatisticsAccumulator::recordDraw(
    uint32_t elementCount,
    uint32_t instanceCount) noexcept
{
    recordSubmittedElements(elementCount, instanceCount);
}

void CommandStatisticsAccumulator::recordIndexedDraw(
    uint32_t indexCount,
    uint32_t instanceCount) noexcept
{
    recordSubmittedElements(indexCount, instanceCount);
}

void CommandStatisticsAccumulator::recordSubmittedElements(
    uint32_t elementCount,
    uint32_t instanceCount) noexcept
{
    m_statistics.drawCalls = saturatingAdd(m_statistics.drawCalls, 1);
    const uint64_t submitted =
        static_cast<uint64_t>(elementCount)
        * static_cast<uint64_t>(instanceCount);
    m_statistics.submittedVertices = saturatingAdd(
        m_statistics.submittedVertices,
        submitted);
}

void CommandStatisticsAccumulator::add(
    const CommandListStatistics& statistics) noexcept
{
    m_statistics.drawCalls = saturatingAdd(
        m_statistics.drawCalls,
        statistics.drawCalls);
    m_statistics.submittedVertices = saturatingAdd(
        m_statistics.submittedVertices,
        statistics.submittedVertices);
}

GpuTimeSample resolveGpuTimeSample(
    bool timestampSupported,
    bool timestampsAvailable,
    uint64_t startTimestamp,
    uint64_t endTimestamp,
    float timestampPeriodNanoseconds,
    uint32_t timestampValidBits) noexcept
{
    if (!timestampSupported
        || timestampValidBits == 0
        || timestampPeriodNanoseconds <= 0.0f) {
        return {GpuTimeStatus::Unsupported, 0.0};
    }
    if (!timestampsAvailable) {
        return {GpuTimeStatus::Waiting, 0.0};
    }

    uint64_t elapsedTicks = 0;
    if (timestampValidBits >= 64) {
        elapsedTicks = endTimestamp - startTimestamp;
    } else {
        const uint64_t timestampMask =
            (uint64_t{1} << timestampValidBits) - 1;
        elapsedTicks = (endTimestamp - startTimestamp) & timestampMask;
    }

    return {
        GpuTimeStatus::Available,
        static_cast<double>(elapsedTicks)
            * static_cast<double>(timestampPeriodNanoseconds)
            / 1'000'000.0,
    };
}

const char* toString(GpuTimeStatus status) noexcept
{
    switch (status) {
        case GpuTimeStatus::Unsupported:
            return "unsupported";
        case GpuTimeStatus::Waiting:
            return "waiting";
        case GpuTimeStatus::Available:
            return "available";
        default:
            return "unknown";
    }
}

bool CompletedFrameStatisticsTracker::recordSubmittedFrame(
    float cpuTimeMilliseconds,
    const CommandListStatistics& commands) noexcept
{
    if (m_pending.has_value()
        || m_lastSubmittedFrame == std::numeric_limits<uint64_t>::max()) {
        return false;
    }

    ++m_lastSubmittedFrame;
    m_pending = PendingFrame{
        m_lastSubmittedFrame,
        cpuTimeMilliseconds,
        commands,
    };
    return true;
}

bool CompletedFrameStatisticsTracker::completePendingFrame(
    const GpuTimeSample& gpuTime) noexcept
{
    if (!m_pending.has_value()) {
        return false;
    }

    m_completed = CompletedFrameStatistics{
        m_pending->submittedFrame,
        m_pending->cpuTimeMilliseconds,
        gpuTime,
        m_pending->commands,
    };
    m_pending.reset();
    return true;
}

} // namespace ku
