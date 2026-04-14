#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace ku {

class RenderPass;

enum class ResourceAccessType {
    Read,
    Write,
};

enum class ResourceHazardType {
    ReadAfterWrite,
    WriteAfterRead,
    WriteAfterWrite,
};

struct ResourceHandle {
    static constexpr uint32_t kInvalidId = UINT32_MAX;

    uint32_t id = kInvalidId;

    [[nodiscard]] bool isValid() const { return id != kInvalidId; }
};

struct ResourceDesc {
    std::string name;
    bool external = false;
};

struct PassResourceAccess {
    ResourceHandle resource;
    ResourceAccessType access = ResourceAccessType::Read;
};

struct PassNode {
    std::string name;
    RenderPass* pass = nullptr;
    std::vector<PassResourceAccess> accesses;
    std::vector<std::string> explicitDependencies;
};

struct PassDependencyEdge {
    size_t fromPass = 0;
    size_t toPass = 0;
    bool explicitDependency = false;
};

struct BarrierPlanItem {
    size_t fromPass = 0;
    size_t toPass = 0;
    ResourceHandle resource;
    ResourceHazardType hazard = ResourceHazardType::ReadAfterWrite;
};

class RenderGraph;

class RenderGraphBuilder {
public:
    RenderGraphBuilder() = default;

    ResourceHandle createResource(std::string_view name);
    ResourceHandle importExternal(std::string_view name);

    void read(ResourceHandle resource);
    void write(ResourceHandle resource);
    void dependsOn(std::string_view passName);

private:
    friend class RenderGraph;

    RenderGraphBuilder(RenderGraph& graph, size_t passIndex);

    RenderGraph* m_graph = nullptr;
    size_t m_passIndex = 0;
};

class RenderGraph {
public:
    void reset();

    [[nodiscard]] size_t registerPass(RenderPass& pass);
    [[nodiscard]] RenderGraphBuilder buildPass(size_t passIndex);

    void compile();

    [[nodiscard]] const std::vector<PassNode>& passes() const { return m_passes; }
    [[nodiscard]] const std::vector<ResourceDesc>& resources() const { return m_resources; }
    [[nodiscard]] const std::vector<size_t>& executionOrder() const { return m_executionOrder; }
    [[nodiscard]] const std::vector<PassDependencyEdge>& dependencies() const { return m_dependencies; }
    [[nodiscard]] const std::vector<BarrierPlanItem>& barrierPlan() const { return m_barrierPlan; }

private:
    friend class RenderGraphBuilder;

    ResourceHandle createOrGetResource(std::string_view name, bool external);
    void addAccess(size_t passIndex, ResourceHandle resource, ResourceAccessType access);
    void addExplicitDependency(size_t passIndex, std::string_view passName);

    std::vector<PassNode> m_passes;
    std::vector<ResourceDesc> m_resources;
    std::vector<size_t> m_executionOrder;
    std::vector<PassDependencyEdge> m_dependencies;
    std::vector<BarrierPlanItem> m_barrierPlan;
};

} // namespace ku
