#include "RenderGraphStatePlanner.h"

#include <algorithm>
#include <array>
#include <limits>
#include <sstream>
#include <stdexcept>

namespace ku {
namespace {

struct TrackedSpan {
    ImageSubresourceRange imageRange{};
    BufferRange bufferRange{};
    ResourceState2 state{};
    bool contentsValid = false;
    bool readers = false;
    bool writer = false;
    ResourceState2 lastWriterState{};
    std::array<VkAccessFlags2, 64> coveredReadAccessByStage{};
    bool hasLastWriter = false;
    std::string skippedWriter;
};

struct TrackedResource {
    StatePlannerResource desc{};
    std::vector<TrackedSpan> spans;
};

constexpr VkAccessFlags2 kWriteAccessMask =
    VK_ACCESS_2_SHADER_WRITE_BIT
    | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT
    | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT
    | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT
    | VK_ACCESS_2_TRANSFER_WRITE_BIT
    | VK_ACCESS_2_HOST_WRITE_BIT
    | VK_ACCESS_2_MEMORY_WRITE_BIT
    | VK_ACCESS_2_TRANSFORM_FEEDBACK_WRITE_BIT_EXT
    | VK_ACCESS_2_TRANSFORM_FEEDBACK_COUNTER_WRITE_BIT_EXT
    | VK_ACCESS_2_COMMAND_PREPROCESS_WRITE_BIT_NV
    | VK_ACCESS_2_ACCELERATION_STRUCTURE_WRITE_BIT_KHR
    | VK_ACCESS_2_MICROMAP_WRITE_BIT_EXT
    | VK_ACCESS_2_OPTICAL_FLOW_WRITE_BIT_NV;

bool stateHasWriter(ResourceState2 state) noexcept
{
    return (state.access & kWriteAccessMask) != 0;
}

bool stateHasReaders(ResourceState2 state) noexcept
{
    return (state.access & ~kWriteAccessMask) != 0;
}

bool intersects(const TrackedSpan& span, const CompiledResourceUse& use)
{
    return use.resource.kind == ResourceKind::Image
        ? RenderGraph::overlaps(span.imageRange, use.imageRange)
        : RenderGraph::overlaps(span.bufferRange, use.bufferRange);
}

void splitBufferSpans(
    std::vector<TrackedSpan>& spans,
    const BufferRange& range)
{
    const VkDeviceSize begin = range.offset;
    const VkDeviceSize end = range.offset + range.size;
    std::vector<TrackedSpan> result;
    result.reserve(spans.size() + 2);
    for (const TrackedSpan& span : spans) {
        const VkDeviceSize spanBegin = span.bufferRange.offset;
        const VkDeviceSize spanEnd = spanBegin + span.bufferRange.size;
        if (end <= spanBegin || begin >= spanEnd) {
            result.push_back(span);
            continue;
        }
        if (spanBegin < begin) {
            TrackedSpan prefix = span;
            prefix.bufferRange = {spanBegin, begin - spanBegin};
            result.push_back(prefix);
        }
        TrackedSpan middle = span;
        const VkDeviceSize middleBegin = std::max(begin, spanBegin);
        const VkDeviceSize middleEnd = std::min(end, spanEnd);
        middle.bufferRange = {middleBegin, middleEnd - middleBegin};
        result.push_back(middle);
        if (end < spanEnd) {
            TrackedSpan suffix = span;
            suffix.bufferRange = {end, spanEnd - end};
            result.push_back(suffix);
        }
    }
    spans = std::move(result);
}

void mergeBufferSpans(std::vector<TrackedSpan>& spans)
{
    if (spans.empty()) return;
    std::vector<TrackedSpan> merged;
    merged.reserve(spans.size());
    for (const TrackedSpan& span : spans) {
        if (!merged.empty()) {
            TrackedSpan& previous = merged.back();
            const bool adjacent = previous.bufferRange.offset
                    + previous.bufferRange.size
                == span.bufferRange.offset;
            if (adjacent && previous.state == span.state
                && previous.contentsValid == span.contentsValid
                && previous.readers == span.readers
                && previous.writer == span.writer
                && previous.lastWriterState == span.lastWriterState
                && previous.coveredReadAccessByStage
                    == span.coveredReadAccessByStage
                && previous.hasLastWriter == span.hasLastWriter
                && previous.skippedWriter == span.skippedWriter) {
                previous.bufferRange.size += span.bufferRange.size;
                continue;
            }
        }
        merged.push_back(span);
    }
    spans = std::move(merged);
}

std::vector<TrackedSpan> makeImageCells(const StatePlannerResource& resource)
{
    std::vector<TrackedSpan> result;
    const VkImageAspectFlags aspects[] = {
        VK_IMAGE_ASPECT_COLOR_BIT,
        VK_IMAGE_ASPECT_DEPTH_BIT,
        VK_IMAGE_ASPECT_STENCIL_BIT,
        VK_IMAGE_ASPECT_METADATA_BIT,
        VK_IMAGE_ASPECT_PLANE_0_BIT,
        VK_IMAGE_ASPECT_PLANE_1_BIT,
        VK_IMAGE_ASPECT_PLANE_2_BIT,
    };
    for (const VkImageAspectFlags aspect : aspects) {
        if ((resource.imageRange.aspect & aspect) == 0) continue;
        for (uint32_t mip = resource.imageRange.baseMipLevel;
             mip < resource.imageRange.baseMipLevel + resource.imageRange.levelCount;
             ++mip) {
            for (uint32_t layer = resource.imageRange.baseArrayLayer;
                 layer < resource.imageRange.baseArrayLayer
                         + resource.imageRange.layerCount;
                 ++layer) {
                result.push_back({
                    {aspect, mip, 1, layer, 1}, {}, resource.initialState,
                    resource.contentsValid,
                    stateHasReaders(resource.initialState),
                    stateHasWriter(resource.initialState)});
                TrackedSpan& span = result.back();
                span.hasLastWriter = span.writer;
                if (span.hasLastWriter) {
                    span.lastWriterState = resource.initialState;
                }
            }
        }
    }
    return result;
}

std::string useDescription(const CompiledResourceUse& use)
{
    if (use.attachment) return "attachment";
    if (use.reads && use.writes) return "read/write";
    if (use.reads) return "read";
    return "write";
}

bool readDomainCovered(
    const TrackedSpan& span,
    const ResourceState2& requiredState) noexcept
{
    for (uint32_t bitIndex = 0; bitIndex < 64; ++bitIndex) {
        const VkPipelineStageFlags2 stageBit =
            VkPipelineStageFlags2{1} << bitIndex;
        if ((requiredState.stages & stageBit) == 0) continue;
        if ((requiredState.access
             & ~span.coveredReadAccessByStage[bitIndex]) != 0) {
            return false;
        }
    }
    return true;
}

void coverReadDomain(
    TrackedSpan& span,
    const ResourceState2& establishedState) noexcept
{
    for (uint32_t bitIndex = 0; bitIndex < 64; ++bitIndex) {
        const VkPipelineStageFlags2 stageBit =
            VkPipelineStageFlags2{1} << bitIndex;
        if ((establishedState.stages & stageBit) == 0) continue;
        span.coveredReadAccessByStage[bitIndex] |= establishedState.access;
    }
}

} // namespace

RenderGraphStatePlan RenderGraphStatePlanner::plan(
    const std::vector<StatePlannerResource>& resources,
    const std::vector<StatePlannerPass>& passes)
{
    std::vector<TrackedResource> tracked;
    tracked.reserve(resources.size());
    for (const StatePlannerResource& resource : resources) {
        if (!resource.handle.isValid()) {
            throw std::invalid_argument("StatePlanner resource handle is invalid");
        }
        TrackedResource item{};
        item.desc = resource;
        if (resource.handle.kind == ResourceKind::Image) {
            item.spans = makeImageCells(resource);
            if (item.spans.empty()) {
                throw std::invalid_argument(
                    "StatePlanner image range has no supported aspect cells");
            }
        } else {
            if (resource.bufferRange.size == 0
                || resource.bufferRange.size == VK_WHOLE_SIZE) {
                throw std::invalid_argument(
                    "StatePlanner buffer range must be normalized and non-zero");
            }
            item.spans.push_back({
                {}, resource.bufferRange, resource.initialState,
                resource.contentsValid,
                stateHasReaders(resource.initialState),
                stateHasWriter(resource.initialState)});
            TrackedSpan& span = item.spans.back();
            span.hasLastWriter = span.writer;
            if (span.hasLastWriter) {
                span.lastWriterState = resource.initialState;
            }
        }
        tracked.push_back(std::move(item));
    }

    RenderGraphStatePlan result{};
    result.barriersBeforePass.resize(passes.size());
    for (size_t passIndex = 0; passIndex < passes.size(); ++passIndex) {
        const StatePlannerPass& pass = passes[passIndex];
        if (!pass.enabled) {
            for (const StatePlannerUse& plannedUse : pass.uses) {
                const CompiledResourceUse& use = plannedUse.use;
                if (!use.writes) continue;
                if (use.resource.index >= tracked.size()
                    || tracked[use.resource.index].desc.handle != use.resource) {
                    throw std::invalid_argument(
                        "StatePlanner use references an unknown resource");
                }
                TrackedResource& resource = tracked[use.resource.index];
                if (use.resource.kind == ResourceKind::Buffer) {
                    splitBufferSpans(resource.spans, use.bufferRange);
                }
                for (TrackedSpan& span : resource.spans) {
                    if (intersects(span, use)) span.skippedWriter = pass.name;
                }
                if (use.resource.kind == ResourceKind::Buffer) {
                    mergeBufferSpans(resource.spans);
                }
            }
            continue;
        }

        for (size_t useIndex = 0; useIndex < pass.uses.size(); ++useIndex) {
            const StatePlannerUse& plannedUse = pass.uses[useIndex];
            const CompiledResourceUse& use = plannedUse.use;
            if (use.resource.index >= tracked.size()
                || tracked[use.resource.index].desc.handle != use.resource) {
                throw std::invalid_argument(
                    "StatePlanner use references an unknown resource");
            }
            TrackedResource& resource = tracked[use.resource.index];
            if (use.resource.kind == ResourceKind::Buffer) {
                splitBufferSpans(resource.spans, use.bufferRange);
            }
            bool touched = false;
            for (TrackedSpan& span : resource.spans) {
                if (!intersects(span, use)) continue;
                touched = true;
                ResourceState2 requiredState = use.state;
                if (use.reads && !use.writes) {
                    for (size_t laterIndex = useIndex + 1;
                         laterIndex < pass.uses.size();
                         ++laterIndex) {
                        const CompiledResourceUse& later =
                            pass.uses[laterIndex].use;
                        if (later.resource != use.resource
                            || !later.reads || later.writes
                            || later.state.layout != use.state.layout
                            || !intersects(span, later)) {
                            continue;
                        }
                        requiredState.stages |= later.state.stages;
                        requiredState.access |= later.state.access;
                    }
                }
                if (use.reads && !span.contentsValid) {
                    std::ostringstream message;
                    message << "RenderGraph preflight rejected pass '" << pass.name
                            << "': resource '" << resource.desc.name << "' "
                            << useDescription(use)
                            << " requires valid contents";
                    if (!span.skippedWriter.empty()) {
                        message << "; skipped producer '"
                                << span.skippedWriter << "'";
                    }
                    throw std::runtime_error(message.str());
                }

                const bool layoutChange = use.resource.kind == ResourceKind::Image
                    && span.state.layout != use.state.layout;
                const bool readerVisibilityMissing = use.reads
                    && span.hasLastWriter
                    && !readDomainCovered(span, requiredState);
                const bool raw = use.reads
                    && (span.writer || readerVisibilityMissing);
                const bool war = use.writes && span.readers;
                const bool waw = use.writes && span.writer;
                const bool hazard = raw || war || waw;
                bool writerIncludedInBarrier = false;
                if (layoutChange || hazard) {
                    ResourceState2 beforeState = span.state;
                    if (raw && span.hasLastWriter) {
                        if (!layoutChange && !war && !waw) {
                            beforeState = span.lastWriterState;
                            beforeState.layout = span.state.layout;
                        } else {
                            beforeState.stages |= span.lastWriterState.stages;
                            beforeState.access |= span.lastWriterState.access;
                        }
                        writerIncludedInBarrier = true;
                    } else if (layoutChange && use.reads
                               && span.hasLastWriter) {
                        // A layout transition must wait for prior readers, while
                        // retaining the direct visibility chain from the last
                        // writer to this reader.
                        beforeState.stages |= span.lastWriterState.stages;
                        beforeState.access |= span.lastWriterState.access;
                        writerIncludedInBarrier = true;
                    }
                    const ResourceHazardType plannedHazard = waw
                        ? ResourceHazardType::WriteAfterWrite
                        : raw ? ResourceHazardType::ReadAfterWrite
                              : ResourceHazardType::WriteAfterRead;
                    result.barriersBeforePass[passIndex].push_back({
                        use.resource,
                        beforeState,
                        requiredState,
                        span.imageRange,
                        span.bufferRange,
                        hazard ? plannedHazard
                               : ResourceHazardType::ReadAfterWrite,
                        hazard,
                    });
                }
                if (use.writes) {
                    span.state = use.state;
                    span.readers = false;
                    span.writer = true;
                    span.lastWriterState = use.state;
                    span.coveredReadAccessByStage = {};
                    span.hasLastWriter = true;
                    span.contentsValid = true;
                    span.skippedWriter.clear();
                } else if (use.reads) {
                    if (span.writer) {
                        span.state = requiredState;
                    } else {
                        span.state.stages |= requiredState.stages;
                        span.state.access |= requiredState.access;
                        span.state.layout = requiredState.layout;
                    }
                    span.readers = true;
                    span.writer = false;
                    if (writerIncludedInBarrier) {
                        coverReadDomain(span, requiredState);
                    }
                }
                if (plannedUse.discardAfter) span.contentsValid = false;
            }
            if (!touched) {
                throw std::invalid_argument(
                    "StatePlanner use range does not intersect its resource");
            }
            if (use.resource.kind == ResourceKind::Buffer) {
                mergeBufferSpans(resource.spans);
            }
        }
    }

    result.finalResources.reserve(tracked.size());
    for (const TrackedResource& resource : tracked) {
        ResourceState2 state{};
        bool contentsValid = true;
        bool first = true;
        for (const TrackedSpan& span : resource.spans) {
            if (first) {
                state = span.state;
                first = false;
            } else if (resource.desc.handle.kind == ResourceKind::Buffer) {
                state.stages |= span.state.stages;
                state.access |= span.state.access;
            } else if (state.layout != span.state.layout) {
                state.layout = VK_IMAGE_LAYOUT_GENERAL;
                state.stages |= span.state.stages;
                state.access |= span.state.access;
            } else {
                state.stages |= span.state.stages;
                state.access |= span.state.access;
            }
            contentsValid &= span.contentsValid;
        }
        result.finalResources.push_back({
            resource.desc.handle, state, contentsValid});
    }
    return result;
}

} // namespace ku
