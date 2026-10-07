#pragma once

#include "RenderGraph.h"

#include <string>
#include <vector>

namespace ku {

struct StatePlannerUse {
    CompiledResourceUse use{};
    bool discardAfter = false;
};

struct StatePlannerPass {
    size_t graphPassIndex = 0;
    std::string name;
    bool enabled = true;
    std::vector<StatePlannerUse> uses;
};

struct StatePlannerResource {
    ResourceHandle handle{};
    std::string name;
    ResourceState2 initialState{};
    bool contentsValid = false;
    ImageSubresourceRange imageRange{};
    BufferRange bufferRange{};
};

struct PlannedResourceBarrier {
    ResourceHandle resource{};
    ResourceState2 before{};
    ResourceState2 after{};
    ImageSubresourceRange imageRange{};
    BufferRange bufferRange{};
    ResourceHazardType hazard = ResourceHazardType::ReadAfterWrite;
    bool hasHazard = false;
};

struct PlannedResourceFinalState {
    ResourceHandle resource{};
    ResourceState2 state{};
    bool contentsValid = false;
};

struct RenderGraphStatePlan {
    std::vector<std::vector<PlannedResourceBarrier>> barriersBeforePass;
    std::vector<PlannedResourceFinalState> finalResources;
};

class RenderGraphStatePlanner {
public:
    [[nodiscard]] static RenderGraphStatePlan plan(
        const std::vector<StatePlannerResource>& resources,
        const std::vector<StatePlannerPass>& passes);
};

} // namespace ku
