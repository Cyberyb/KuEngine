// KuEngine frame statistics: draw accumulation, GPU timing state, and completed-submit snapshots.
#pragma once

#include <cstdint>
#include <optional>

namespace ku {

struct CommandListStatistics {
    uint64_t drawCalls = 0;
    uint64_t submittedVertices = 0;
};

[[nodiscard]] bool commandStatisticsEqual(
    const CommandListStatistics& left,
    const CommandListStatistics& right) noexcept;

class CommandStatisticsAccumulator {
public:
    void reset() noexcept { m_statistics = {}; }
    void recordDraw(uint32_t elementCount, uint32_t instanceCount) noexcept;
    void recordIndexedDraw(
        uint32_t indexCount,
        uint32_t instanceCount) noexcept;
    void add(const CommandListStatistics& statistics) noexcept;

    [[nodiscard]] const CommandListStatistics& statistics() const noexcept
    {
        return m_statistics;
    }

private:
    void recordSubmittedElements(
        uint32_t elementCount,
        uint32_t instanceCount) noexcept;
    CommandListStatistics m_statistics{};
};

enum class GpuTimeStatus : uint8_t {
    Unsupported,
    Waiting,
    Available,
};

struct GpuTimeSample {
    GpuTimeStatus status = GpuTimeStatus::Unsupported;
    double milliseconds = 0.0;
};

[[nodiscard]] GpuTimeSample resolveGpuTimeSample(
    bool timestampSupported,
    bool timestampsAvailable,
    uint64_t startTimestamp,
    uint64_t endTimestamp,
    float timestampPeriodNanoseconds,
    uint32_t timestampValidBits) noexcept;

[[nodiscard]] const char* toString(GpuTimeStatus status) noexcept;

struct CompletedFrameStatistics {
    uint64_t submittedFrame = 0;
    float cpuTimeMilliseconds = 0.0f;
    GpuTimeSample gpuTime{};
    CommandListStatistics commands{};
};

// The current Runtime has one frame in flight, so at most one submitted frame
// may be awaiting Fence completion. Skipped acquire/resize loops never call
// recordSubmittedFrame and therefore cannot advance this sequence.
class CompletedFrameStatisticsTracker {
public:
    [[nodiscard]] bool recordSubmittedFrame(
        float cpuTimeMilliseconds,
        const CommandListStatistics& commands) noexcept;
    [[nodiscard]] bool completePendingFrame(
        const GpuTimeSample& gpuTime) noexcept;

    [[nodiscard]] bool hasPendingFrame() const noexcept
    {
        return m_pending.has_value();
    }
    [[nodiscard]] const std::optional<CompletedFrameStatistics>& completedFrame() const noexcept
    {
        return m_completed;
    }

private:
    struct PendingFrame {
        uint64_t submittedFrame = 0;
        float cpuTimeMilliseconds = 0.0f;
        CommandListStatistics commands{};
    };

    uint64_t m_lastSubmittedFrame = 0;
    std::optional<PendingFrame> m_pending;
    std::optional<CompletedFrameStatistics> m_completed;
};

} // namespace ku
