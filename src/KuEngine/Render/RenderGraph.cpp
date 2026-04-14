#include "RenderGraph.h"

#include "RenderPass.h"

#include <algorithm>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <unordered_map>

namespace ku {

namespace {

constexpr uint8_t kReadMask = 0x1;
constexpr uint8_t kWriteMask = 0x2;

uint64_t makeEdgeKey(size_t fromPass, size_t toPass)
{
    return (static_cast<uint64_t>(fromPass) << 32u) | static_cast<uint64_t>(toPass);
}

bool hasRead(uint8_t mode)
{
    return (mode & kReadMask) != 0;
}

bool hasWrite(uint8_t mode)
{
    return (mode & kWriteMask) != 0;
}

ResourceHazardType classifyHazard(uint8_t sourceMode, uint8_t targetMode)
{
    const bool sourceWrite = hasWrite(sourceMode);
    const bool targetWrite = hasWrite(targetMode);
    const bool sourceRead = hasRead(sourceMode);
    const bool targetRead = hasRead(targetMode);

    if (sourceWrite && targetWrite) {
        return ResourceHazardType::WriteAfterWrite;
    }
    if (sourceWrite && targetRead) {
        return ResourceHazardType::ReadAfterWrite;
    }
    if (sourceRead && targetWrite) {
        return ResourceHazardType::WriteAfterRead;
    }

    throw std::logic_error("classifyHazard called without a write hazard");
}

void addDependencyEdge(
    std::vector<PassDependencyEdge>& dependencies,
    std::unordered_map<uint64_t, size_t>& dependencyLookup,
    size_t fromPass,
    size_t toPass,
    bool explicitDependency)
{
    if (fromPass == toPass) {
        throw std::runtime_error("RenderGraph dependency cannot point to the same pass");
    }

    const uint64_t key = makeEdgeKey(fromPass, toPass);
    const auto found = dependencyLookup.find(key);
    if (found != dependencyLookup.end()) {
        if (explicitDependency) {
            dependencies[found->second].explicitDependency = true;
        }
        return;
    }

    dependencyLookup[key] = dependencies.size();
    dependencies.push_back(PassDependencyEdge{fromPass, toPass, explicitDependency});
}

} // namespace

RenderGraphBuilder::RenderGraphBuilder(RenderGraph& graph, size_t passIndex)
    : m_graph(&graph)
    , m_passIndex(passIndex)
{
}

ResourceHandle RenderGraphBuilder::createResource(std::string_view name)
{
    if (m_graph == nullptr) {
        throw std::runtime_error("RenderGraphBuilder is not bound to a graph");
    }

    return m_graph->createOrGetResource(name, false);
}

ResourceHandle RenderGraphBuilder::importExternal(std::string_view name)
{
    if (m_graph == nullptr) {
        throw std::runtime_error("RenderGraphBuilder is not bound to a graph");
    }

    return m_graph->createOrGetResource(name, true);
}

void RenderGraphBuilder::read(ResourceHandle resource)
{
    if (m_graph == nullptr) {
        throw std::runtime_error("RenderGraphBuilder is not bound to a graph");
    }

    m_graph->addAccess(m_passIndex, resource, ResourceAccessType::Read);
}

void RenderGraphBuilder::write(ResourceHandle resource)
{
    if (m_graph == nullptr) {
        throw std::runtime_error("RenderGraphBuilder is not bound to a graph");
    }

    m_graph->addAccess(m_passIndex, resource, ResourceAccessType::Write);
}

void RenderGraphBuilder::dependsOn(std::string_view passName)
{
    if (m_graph == nullptr) {
        throw std::runtime_error("RenderGraphBuilder is not bound to a graph");
    }

    m_graph->addExplicitDependency(m_passIndex, passName);
}

void RenderGraph::reset()
{
    m_passes.clear();
    m_resources.clear();
    m_executionOrder.clear();
    m_dependencies.clear();
    m_barrierPlan.clear();
}

size_t RenderGraph::registerPass(RenderPass& pass)
{
    m_passes.push_back(PassNode{std::string(pass.name()), &pass, {}});
    return m_passes.size() - 1;
}

RenderGraphBuilder RenderGraph::buildPass(size_t passIndex)
{
    if (passIndex >= m_passes.size()) {
        throw std::out_of_range("RenderGraph::buildPass passIndex out of range");
    }

    return RenderGraphBuilder(*this, passIndex);
}

void RenderGraph::compile()
{
    m_executionOrder.clear();
    m_dependencies.clear();
    m_barrierPlan.clear();

    const size_t passCount = m_passes.size();
    if (passCount == 0) {
        return;
    }

    std::unordered_map<uint64_t, size_t> dependencyLookup;

    std::unordered_map<std::string, size_t> passNameToIndex;
    passNameToIndex.reserve(passCount);
    for (size_t i = 0; i < passCount; ++i) {
        const auto [it, inserted] = passNameToIndex.emplace(m_passes[i].name, i);
        if (!inserted) {
            throw std::runtime_error("RenderGraph pass name duplicated: " + m_passes[i].name);
        }
    }

    for (size_t targetPass = 0; targetPass < passCount; ++targetPass) {
        for (const std::string& dependencyName : m_passes[targetPass].explicitDependencies) {
            const auto found = passNameToIndex.find(dependencyName);
            if (found == passNameToIndex.end()) {
                throw std::runtime_error(
                    "RenderGraph explicit dependency not found: pass '" + m_passes[targetPass].name +
                    "' depends on '" + dependencyName + "'");
            }

            addDependencyEdge(
                m_dependencies,
                dependencyLookup,
                found->second,
                targetPass,
                true);
        }
    }

    std::unordered_map<uint32_t, std::map<size_t, uint8_t>> resourceUsage;
    for (size_t passIndex = 0; passIndex < passCount; ++passIndex) {
        for (const PassResourceAccess& access : m_passes[passIndex].accesses) {
            if (!access.resource.isValid() || access.resource.id >= m_resources.size()) {
                throw std::runtime_error("RenderGraph compile failed: invalid resource handle in pass access");
            }

            uint8_t& mode = resourceUsage[access.resource.id][passIndex];
            if (access.access == ResourceAccessType::Read) {
                mode |= kReadMask;
            } else {
                mode |= kWriteMask;
            }
        }
    }

    for (const auto& [resourceId, usageByPass] : resourceUsage) {
        std::vector<std::pair<size_t, uint8_t>> orderedUsage;
        orderedUsage.reserve(usageByPass.size());
        for (const auto& usage : usageByPass) {
            orderedUsage.push_back(usage);
        }

        for (size_t i = 0; i < orderedUsage.size(); ++i) {
            const size_t fromPass = orderedUsage[i].first;
            const uint8_t fromMode = orderedUsage[i].second;

            for (size_t j = i + 1; j < orderedUsage.size(); ++j) {
                const size_t toPass = orderedUsage[j].first;
                const uint8_t toMode = orderedUsage[j].second;

                const bool writeHazard = hasWrite(fromMode) || hasWrite(toMode);
                if (!writeHazard) {
                    continue;
                }

                addDependencyEdge(
                    m_dependencies,
                    dependencyLookup,
                    fromPass,
                    toPass,
                    false);

                m_barrierPlan.push_back(BarrierPlanItem{
                    fromPass,
                    toPass,
                    ResourceHandle{resourceId},
                    classifyHazard(fromMode, toMode)});
            }
        }
    }

    std::vector<size_t> inDegree(passCount, 0);
    std::vector<std::vector<size_t>> adjacency(passCount);
    for (const PassDependencyEdge& dependency : m_dependencies) {
        adjacency[dependency.fromPass].push_back(dependency.toPass);
        ++inDegree[dependency.toPass];
    }

    std::set<size_t> ready;
    for (size_t i = 0; i < passCount; ++i) {
        if (inDegree[i] == 0) {
            ready.insert(i);
        }
    }

    while (!ready.empty()) {
        const size_t pass = *ready.begin();
        ready.erase(ready.begin());
        m_executionOrder.push_back(pass);

        for (const size_t next : adjacency[pass]) {
            if (inDegree[next] == 0) {
                continue;
            }

            --inDegree[next];
            if (inDegree[next] == 0) {
                ready.insert(next);
            }
        }
    }

    if (m_executionOrder.size() != passCount) {
        std::ostringstream oss;
        bool first = true;
        for (size_t i = 0; i < passCount; ++i) {
            if (inDegree[i] == 0) {
                continue;
            }
            if (!first) {
                oss << ", ";
            }
            oss << m_passes[i].name;
            first = false;
        }

        throw std::runtime_error(
            "RenderGraph cycle detected among passes: " +
            (first ? std::string("<unknown>") : oss.str()));
    }
}

ResourceHandle RenderGraph::createOrGetResource(std::string_view name, bool external)
{
    if (name.empty()) {
        throw std::invalid_argument("RenderGraph resource name cannot be empty");
    }

    const auto it = std::find_if(
        m_resources.begin(),
        m_resources.end(),
        [&](const ResourceDesc& desc) {
            return desc.name == name;
        });

    if (it != m_resources.end()) {
        const size_t index = static_cast<size_t>(std::distance(m_resources.begin(), it));
        if (external) {
            m_resources[index].external = true;
        }
        return ResourceHandle{static_cast<uint32_t>(index)};
    }

    m_resources.push_back(ResourceDesc{std::string(name), external});
    return ResourceHandle{static_cast<uint32_t>(m_resources.size() - 1)};
}

void RenderGraph::addAccess(size_t passIndex, ResourceHandle resource, ResourceAccessType access)
{
    if (passIndex >= m_passes.size()) {
        throw std::out_of_range("RenderGraph::addAccess passIndex out of range");
    }
    if (!resource.isValid() || resource.id >= m_resources.size()) {
        throw std::invalid_argument("RenderGraph::addAccess resource handle is invalid");
    }

    auto& accesses = m_passes[passIndex].accesses;
    const auto duplicate = std::find_if(
        accesses.begin(),
        accesses.end(),
        [&](const PassResourceAccess& current) {
            return current.resource.id == resource.id && current.access == access;
        });

    if (duplicate != accesses.end()) {
        return;
    }

    accesses.push_back(PassResourceAccess{resource, access});
}

void RenderGraph::addExplicitDependency(size_t passIndex, std::string_view passName)
{
    if (passIndex >= m_passes.size()) {
        throw std::out_of_range("RenderGraph::addExplicitDependency passIndex out of range");
    }
    if (passName.empty()) {
        throw std::invalid_argument("RenderGraph explicit dependency name cannot be empty");
    }

    auto& dependencies = m_passes[passIndex].explicitDependencies;
    const auto found = std::find(dependencies.begin(), dependencies.end(), passName);
    if (found != dependencies.end()) {
        return;
    }

    dependencies.emplace_back(passName);
}

} // namespace ku
