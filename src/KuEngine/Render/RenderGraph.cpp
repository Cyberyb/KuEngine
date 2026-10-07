#include "RenderGraph.h"

#include "RenderPass.h"

#include <algorithm>
#include <atomic>
#include <cstring>
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <unordered_map>

namespace ku {
namespace {

constexpr uint8_t kReadMask = 0x1;
constexpr uint8_t kWriteMask = 0x2;
std::atomic<uint64_t> g_nextGraphGeneration{1};

uint64_t nextGraphGeneration()
{
    const uint64_t generation = g_nextGraphGeneration.fetch_add(1);
    if (generation == 0 || generation == std::numeric_limits<uint64_t>::max()) {
        throw std::overflow_error("RenderGraph generation space exhausted");
    }
    return generation;
}

uint64_t makeEdgeKey(size_t fromPass, size_t toPass)
{
    return (static_cast<uint64_t>(fromPass) << 32u)
        | static_cast<uint64_t>(toPass);
}

bool hasRead(uint8_t mode) { return (mode & kReadMask) != 0; }
bool hasWrite(uint8_t mode) { return (mode & kWriteMask) != 0; }

ResourceHazardType classifyHazard(uint8_t sourceMode, uint8_t targetMode)
{
    if (hasWrite(sourceMode) && hasWrite(targetMode)) {
        return ResourceHazardType::WriteAfterWrite;
    }
    if (hasWrite(sourceMode) && hasRead(targetMode)) {
        return ResourceHazardType::ReadAfterWrite;
    }
    if (hasRead(sourceMode) && hasWrite(targetMode)) {
        return ResourceHazardType::WriteAfterRead;
    }
    throw std::logic_error("RenderGraph hazard classification requires a write");
}

void addDependencyEdge(
    std::vector<PassDependencyEdge>& dependencies,
    std::unordered_map<uint64_t, size_t>& lookup,
    size_t fromPass,
    size_t toPass,
    bool explicitDependency)
{
    if (fromPass == toPass) {
        throw std::runtime_error(
            "RenderGraph dependency cannot point to the same pass");
    }
    const uint64_t key = makeEdgeKey(fromPass, toPass);
    const auto found = lookup.find(key);
    if (found != lookup.end()) {
        if (explicitDependency) {
            dependencies[found->second].explicitDependency = true;
        }
        return;
    }
    lookup.emplace(key, dependencies.size());
    dependencies.push_back({fromPass, toPass, explicitDependency});
}

const PassAttachment* findAttachment(const PassNode& pass, uint32_t index)
{
    const auto found = std::find_if(
        pass.attachments.begin(),
        pass.attachments.end(),
        [index](const PassAttachment& attachment) {
            return attachment.resource.index == index;
        });
    return found == pass.attachments.end() ? nullptr : &*found;
}

bool descriptionsEqual(const ResourceDesc& lhs, const ResourceDesc& rhs)
{
    if (lhs.kind != rhs.kind || lhs.ownership != rhs.ownership
        || lhs.description.index() != rhs.description.index()) {
        return false;
    }
    if (lhs.kind == ResourceKind::Image) {
        return std::get<ImageDesc>(lhs.description)
            == std::get<ImageDesc>(rhs.description);
    }
    return std::get<BufferDesc>(lhs.description)
        == std::get<BufferDesc>(rhs.description);
}

struct UseSemantics {
    ResourceState2 state{};
    bool reads = false;
    bool writes = false;
    VkImageUsageFlags imageUsage = 0;
    VkBufferUsageFlags bufferUsage = 0;
};

UseSemantics imageUseSemantics(ImageUse use)
{
    switch (use) {
        case ImageUse::ColorAttachment:
            return {{VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                     VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT
                         | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                     VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},
                    true, true, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, 0};
        case ImageUse::DepthAttachment:
            return {{VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT
                         | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
                     VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT
                         | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                     VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL},
                    true, true, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, 0};
        case ImageUse::FragmentSampled:
            return {{VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                     VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                     VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
                    true, false, VK_IMAGE_USAGE_SAMPLED_BIT, 0};
        case ImageUse::FragmentStorageRead:
            return {{VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                     VK_ACCESS_2_SHADER_STORAGE_READ_BIT,
                     VK_IMAGE_LAYOUT_GENERAL},
                    true, false, VK_IMAGE_USAGE_STORAGE_BIT, 0};
        case ImageUse::FragmentStorageWrite:
            return {{VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                     VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                     VK_IMAGE_LAYOUT_GENERAL},
                    false, true, VK_IMAGE_USAGE_STORAGE_BIT, 0};
        case ImageUse::ComputeSampled:
            return {{VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                     VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                     VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
                    true, false, VK_IMAGE_USAGE_SAMPLED_BIT, 0};
        case ImageUse::ComputeStorageRead:
            return {{VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                     VK_ACCESS_2_SHADER_STORAGE_READ_BIT,
                     VK_IMAGE_LAYOUT_GENERAL},
                    true, false, VK_IMAGE_USAGE_STORAGE_BIT, 0};
        case ImageUse::ComputeStorageWrite:
            return {{VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                     VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                     VK_IMAGE_LAYOUT_GENERAL},
                    false, true, VK_IMAGE_USAGE_STORAGE_BIT, 0};
        case ImageUse::ComputeStorageReadWrite:
            return {{VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                     VK_ACCESS_2_SHADER_STORAGE_READ_BIT
                         | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                     VK_IMAGE_LAYOUT_GENERAL},
                    true, true, VK_IMAGE_USAGE_STORAGE_BIT, 0};
        case ImageUse::TransferSource:
            return {{VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                     VK_ACCESS_2_TRANSFER_READ_BIT,
                     VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL},
                    true, false, VK_IMAGE_USAGE_TRANSFER_SRC_BIT, 0};
        case ImageUse::TransferDestination:
            return {{VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                     VK_ACCESS_2_TRANSFER_WRITE_BIT,
                     VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL},
                    false, true, VK_IMAGE_USAGE_TRANSFER_DST_BIT, 0};
    }
    throw std::logic_error("unknown ImageUse");
}

UseSemantics bufferUseSemantics(BufferUse use)
{
    switch (use) {
        case BufferUse::Vertex:
            return {{VK_PIPELINE_STAGE_2_VERTEX_ATTRIBUTE_INPUT_BIT,
                     VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT,
                     VK_IMAGE_LAYOUT_UNDEFINED},
                    true, false, 0, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT};
        case BufferUse::Index:
            return {{VK_PIPELINE_STAGE_2_INDEX_INPUT_BIT,
                     VK_ACCESS_2_INDEX_READ_BIT,
                     VK_IMAGE_LAYOUT_UNDEFINED},
                    true, false, 0, VK_BUFFER_USAGE_INDEX_BUFFER_BIT};
        case BufferUse::VertexUniform:
            return {{VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT,
                     VK_ACCESS_2_UNIFORM_READ_BIT,
                     VK_IMAGE_LAYOUT_UNDEFINED},
                    true, false, 0, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT};
        case BufferUse::FragmentUniform:
            return {{VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                     VK_ACCESS_2_UNIFORM_READ_BIT,
                     VK_IMAGE_LAYOUT_UNDEFINED},
                    true, false, 0, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT};
        case BufferUse::Indirect:
            return {{VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT,
                     VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT,
                     VK_IMAGE_LAYOUT_UNDEFINED},
                    true, false, 0, VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT};
        case BufferUse::FragmentStorageRead:
            return {{VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                     VK_ACCESS_2_SHADER_STORAGE_READ_BIT,
                     VK_IMAGE_LAYOUT_UNDEFINED},
                    true, false, 0, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT};
        case BufferUse::FragmentStorageWrite:
            return {{VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                     VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                     VK_IMAGE_LAYOUT_UNDEFINED},
                    false, true, 0, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT};
        case BufferUse::ComputeUniform:
            return {{VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                     VK_ACCESS_2_UNIFORM_READ_BIT,
                     VK_IMAGE_LAYOUT_UNDEFINED},
                    true, false, 0, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT};
        case BufferUse::ComputeStorageRead:
            return {{VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                     VK_ACCESS_2_SHADER_STORAGE_READ_BIT,
                     VK_IMAGE_LAYOUT_UNDEFINED},
                    true, false, 0, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT};
        case BufferUse::ComputeStorageWrite:
            return {{VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                     VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                     VK_IMAGE_LAYOUT_UNDEFINED},
                    false, true, 0, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT};
        case BufferUse::ComputeStorageReadWrite:
            return {{VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                     VK_ACCESS_2_SHADER_STORAGE_READ_BIT
                         | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                     VK_IMAGE_LAYOUT_UNDEFINED},
                    true, true, 0, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT};
        case BufferUse::TransferSource:
            return {{VK_PIPELINE_STAGE_2_COPY_BIT,
                     VK_ACCESS_2_TRANSFER_READ_BIT,
                     VK_IMAGE_LAYOUT_UNDEFINED},
                    true, false, 0, VK_BUFFER_USAGE_TRANSFER_SRC_BIT};
        case BufferUse::TransferDestination:
            return {{VK_PIPELINE_STAGE_2_COPY_BIT
                         | VK_PIPELINE_STAGE_2_CLEAR_BIT,
                     VK_ACCESS_2_TRANSFER_WRITE_BIT,
                     VK_IMAGE_LAYOUT_UNDEFINED},
                    false, true, 0, VK_BUFFER_USAGE_TRANSFER_DST_BIT};
    }
    throw std::logic_error("unknown BufferUse");
}

} // namespace

bool operator==(const ImageDesc& lhs, const ImageDesc& rhs) noexcept
{
    return lhs.extent == rhs.extent
        && lhs.format == rhs.format
        && lhs.usage == rhs.usage
        && lhs.aspect == rhs.aspect
        && lhs.mipLevels == rhs.mipLevels
        && lhs.arrayLayers == rhs.arrayLayers
        && lhs.samples == rhs.samples
        && std::memcmp(
            &lhs.clearValue,
            &rhs.clearValue,
            sizeof(VkClearValue)) == 0
        && lhs.initialContent == rhs.initialContent;
}

ResourceHandle resourceHandle(ImageHandle handle) noexcept
{
    return {handle.index, handle.graphGeneration, ResourceKind::Image};
}

ResourceHandle resourceHandle(BufferHandle handle) noexcept
{
    return {handle.index, handle.graphGeneration, ResourceKind::Buffer};
}

RenderGraphBuilder::RenderGraphBuilder(RenderGraph& graph, size_t passIndex)
    : m_graph(&graph), m_passIndex(passIndex)
{
}

ImageHandle RenderGraphBuilder::createImage(
    std::string_view name,
    const ImageDesc& desc)
{
    if (!m_graph) throw std::runtime_error("RenderGraphBuilder is not bound");
    return m_graph->createOrGetImage(name, ResourceOwnership::Internal, desc);
}

ImageHandle RenderGraphBuilder::importImage(
    std::string_view name,
    const ImageDesc& desc)
{
    if (!m_graph) throw std::runtime_error("RenderGraphBuilder is not bound");
    return m_graph->createOrGetImage(name, ResourceOwnership::Imported, desc);
}

BufferHandle RenderGraphBuilder::createBuffer(
    std::string_view name,
    const BufferDesc& desc)
{
    if (!m_graph) throw std::runtime_error("RenderGraphBuilder is not bound");
    return m_graph->createOrGetBuffer(name, ResourceOwnership::Internal, desc);
}

BufferHandle RenderGraphBuilder::importBuffer(
    std::string_view name,
    const BufferDesc& desc)
{
    if (!m_graph) throw std::runtime_error("RenderGraphBuilder is not bound");
    return m_graph->createOrGetBuffer(name, ResourceOwnership::Imported, desc);
}

void RenderGraphBuilder::useImage(
    ImageHandle resource,
    ImageUse use,
    ImageSubresourceRange range)
{
    if (!m_graph) throw std::runtime_error("RenderGraphBuilder is not bound");
    m_graph->addImageUse(m_passIndex, resource, use, range);
}

void RenderGraphBuilder::useBuffer(
    BufferHandle resource,
    BufferUse use,
    BufferRange range)
{
    if (!m_graph) throw std::runtime_error("RenderGraphBuilder is not bound");
    m_graph->addBufferUse(m_passIndex, resource, use, range);
}

void RenderGraphBuilder::colorAttachment(
    ImageHandle resource,
    AttachmentLoadPolicy loadPolicy,
    AttachmentStorePolicy storePolicy)
{
    if (!m_graph) throw std::runtime_error("RenderGraphBuilder is not bound");
    m_graph->addAttachment(
        m_passIndex,
        resource,
        AttachmentType::Color,
        loadPolicy,
        storePolicy);
}

void RenderGraphBuilder::depthAttachment(
    ImageHandle resource,
    AttachmentLoadPolicy loadPolicy,
    AttachmentStorePolicy storePolicy)
{
    if (!m_graph) throw std::runtime_error("RenderGraphBuilder is not bound");
    m_graph->addAttachment(
        m_passIndex,
        resource,
        AttachmentType::Depth,
        loadPolicy,
        storePolicy);
}

ImageHandle RenderGraphBuilder::exportImage(ImageHandle resource)
{
    if (!m_graph) throw std::runtime_error("RenderGraphBuilder is not bound");
    m_graph->markExported(resourceHandle(resource));
    return resource;
}

BufferHandle RenderGraphBuilder::exportBuffer(BufferHandle resource)
{
    if (!m_graph) throw std::runtime_error("RenderGraphBuilder is not bound");
    m_graph->markExported(resourceHandle(resource));
    return resource;
}

void RenderGraphBuilder::dependsOn(std::string_view passName)
{
    if (!m_graph) throw std::runtime_error("RenderGraphBuilder is not bound");
    m_graph->addExplicitDependency(m_passIndex, passName);
}

RenderGraph::RenderGraph()
    : m_generation(nextGraphGeneration())
{
}

void RenderGraph::reset()
{
    m_generation = nextGraphGeneration();
    m_passes.clear();
    m_resources.clear();
    m_executionOrder.clear();
    m_dependencies.clear();
    m_barrierPlan.clear();
}

size_t RenderGraph::registerPass(RenderPass& pass)
{
    m_passes.push_back({std::string(pass.name()), &pass, {}, {}, {}});
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
    validateResourcesAndUses();

    const size_t passCount = m_passes.size();
    std::vector<PassDependencyEdge> dependencies;
    std::vector<BarrierPlanItem> barriers;
    std::vector<size_t> executionOrder;
    std::unordered_map<uint64_t, size_t> dependencyLookup;
    std::unordered_map<std::string, size_t> passNameToIndex;
    passNameToIndex.reserve(passCount);

    for (size_t i = 0; i < passCount; ++i) {
        if (m_passes[i].name.empty()) {
            throw std::runtime_error("RenderGraph pass name cannot be empty");
        }
        const auto [unused, inserted] =
            passNameToIndex.emplace(m_passes[i].name, i);
        (void)unused;
        if (!inserted) {
            throw std::runtime_error(
                "RenderGraph pass name duplicated: " + m_passes[i].name);
        }
    }

    for (size_t target = 0; target < passCount; ++target) {
        for (const std::string& dependencyName
             : m_passes[target].explicitDependencies) {
            const auto found = passNameToIndex.find(dependencyName);
            if (found == passNameToIndex.end()) {
                throw std::runtime_error(
                    "RenderGraph explicit dependency not found: pass '"
                    + m_passes[target].name + "' depends on '"
                    + dependencyName + "'");
            }
            addDependencyEdge(
                dependencies,
                dependencyLookup,
                found->second,
                target,
                true);
        }
    }

    for (size_t sourcePass = 0; sourcePass < passCount; ++sourcePass) {
        for (size_t targetPass = sourcePass + 1;
             targetPass < passCount;
             ++targetPass) {
            std::set<uint32_t> plannedResources;
            for (const CompiledResourceUse& sourceUse
                 : m_passes[sourcePass].uses) {
                for (const CompiledResourceUse& targetUse
                     : m_passes[targetPass].uses) {
                    if (sourceUse.resource != targetUse.resource) continue;
                    const bool rangeOverlap = sourceUse.resource.kind
                            == ResourceKind::Image
                        ? overlaps(sourceUse.imageRange, targetUse.imageRange)
                        : overlaps(sourceUse.bufferRange, targetUse.bufferRange);
                    if (!rangeOverlap
                        || (!sourceUse.writes && !targetUse.writes)) {
                        continue;
                    }
                    addDependencyEdge(
                        dependencies,
                        dependencyLookup,
                        sourcePass,
                        targetPass,
                        false);
                    if (!plannedResources.insert(sourceUse.resource.index).second) {
                        continue;
                    }
                    const uint8_t sourceMode =
                        (sourceUse.reads ? kReadMask : 0)
                        | (sourceUse.writes ? kWriteMask : 0);
                    const uint8_t targetMode =
                        (targetUse.reads ? kReadMask : 0)
                        | (targetUse.writes ? kWriteMask : 0);
                    barriers.push_back({
                        sourcePass,
                        targetPass,
                        sourceUse.resource,
                        classifyHazard(sourceMode, targetMode),
                    });
                }
            }
        }
    }

    std::vector<size_t> inDegree(passCount, 0);
    std::vector<std::vector<size_t>> adjacency(passCount);
    for (const PassDependencyEdge& dependency : dependencies) {
        adjacency[dependency.fromPass].push_back(dependency.toPass);
        ++inDegree[dependency.toPass];
    }
    std::set<size_t> ready;
    for (size_t i = 0; i < passCount; ++i) {
        if (inDegree[i] == 0) ready.insert(i);
    }
    while (!ready.empty()) {
        const size_t pass = *ready.begin();
        ready.erase(ready.begin());
        executionOrder.push_back(pass);
        for (const size_t next : adjacency[pass]) {
            if (--inDegree[next] == 0) ready.insert(next);
        }
    }
    if (executionOrder.size() != passCount) {
        std::ostringstream names;
        for (size_t i = 0; i < passCount; ++i) {
            if (inDegree[i] == 0) continue;
            if (names.tellp() > 0) names << ", ";
            names << m_passes[i].name;
        }
        throw std::runtime_error(
            "RenderGraph cycle detected among passes: " + names.str());
    }

    m_dependencies = std::move(dependencies);
    m_barrierPlan = std::move(barriers);
    m_executionOrder = std::move(executionOrder);
}

ImageHandle RenderGraph::createOrGetImage(
    std::string_view name,
    ResourceOwnership ownership,
    const ImageDesc& desc)
{
    if (name.empty()) {
        throw std::invalid_argument("RenderGraph resource name cannot be empty");
    }
    ResourceDesc requested{
        std::string(name), ResourceKind::Image, ownership, desc, false};
    const auto found = std::find_if(
        m_resources.begin(), m_resources.end(),
        [name](const ResourceDesc& current) { return current.name == name; });
    if (found != m_resources.end()) {
        if (!descriptionsEqual(*found, requested)) {
            throw std::runtime_error(
                "RenderGraph resource declaration conflicts with existing name: "
                + std::string(name));
        }
        return {
            static_cast<uint32_t>(std::distance(m_resources.begin(), found)),
            m_generation};
    }
    if (m_resources.size() >= std::numeric_limits<uint32_t>::max()) {
        throw std::overflow_error("RenderGraph resource count exceeds uint32_t");
    }
    m_resources.push_back(std::move(requested));
    return {static_cast<uint32_t>(m_resources.size() - 1), m_generation};
}

BufferHandle RenderGraph::createOrGetBuffer(
    std::string_view name,
    ResourceOwnership ownership,
    const BufferDesc& desc)
{
    if (name.empty()) {
        throw std::invalid_argument("RenderGraph resource name cannot be empty");
    }
    ResourceDesc requested{
        std::string(name), ResourceKind::Buffer, ownership, desc, false};
    const auto found = std::find_if(
        m_resources.begin(), m_resources.end(),
        [name](const ResourceDesc& current) { return current.name == name; });
    if (found != m_resources.end()) {
        if (!descriptionsEqual(*found, requested)) {
            throw std::runtime_error(
                "RenderGraph resource declaration conflicts with existing name: "
                + std::string(name));
        }
        return {
            static_cast<uint32_t>(std::distance(m_resources.begin(), found)),
            m_generation};
    }
    if (m_resources.size() >= std::numeric_limits<uint32_t>::max()) {
        throw std::overflow_error("RenderGraph resource count exceeds uint32_t");
    }
    m_resources.push_back(std::move(requested));
    return {static_cast<uint32_t>(m_resources.size() - 1), m_generation};
}

ImageSubresourceRange RenderGraph::normalizeImageRange(
    const ImageDesc& desc,
    ImageSubresourceRange range)
{
    if (range.aspect == 0) range.aspect = desc.aspect;
    if ((range.aspect & desc.aspect) != range.aspect || range.aspect == 0) {
        throw std::invalid_argument(
            "RenderGraph image range aspect is not contained in descriptor");
    }
    if (range.levelCount == VK_REMAINING_MIP_LEVELS) {
        if (range.baseMipLevel >= desc.mipLevels) {
            throw std::out_of_range("RenderGraph image mip base is out of range");
        }
        range.levelCount = desc.mipLevels - range.baseMipLevel;
    }
    if (range.layerCount == VK_REMAINING_ARRAY_LAYERS) {
        if (range.baseArrayLayer >= desc.arrayLayers) {
            throw std::out_of_range("RenderGraph image layer base is out of range");
        }
        range.layerCount = desc.arrayLayers - range.baseArrayLayer;
    }
    if (range.levelCount == 0 || range.layerCount == 0
        || range.baseMipLevel >= desc.mipLevels
        || range.levelCount > desc.mipLevels - range.baseMipLevel
        || range.baseArrayLayer >= desc.arrayLayers
        || range.layerCount > desc.arrayLayers - range.baseArrayLayer) {
        throw std::out_of_range("RenderGraph image subresource range is invalid");
    }
    return range;
}

BufferRange RenderGraph::normalizeBufferRange(
    const BufferDesc& desc,
    BufferRange range)
{
    if (range.offset >= desc.size) {
        throw std::out_of_range("RenderGraph buffer range offset is out of range");
    }
    if (range.size == VK_WHOLE_SIZE) range.size = desc.size - range.offset;
    if (range.size == 0 || range.size > desc.size - range.offset) {
        throw std::out_of_range(
            "RenderGraph buffer range is zero, overflowing, or out of range");
    }
    return range;
}

bool RenderGraph::overlaps(
    const ImageSubresourceRange& lhs,
    const ImageSubresourceRange& rhs) noexcept
{
    return (lhs.aspect & rhs.aspect) != 0
        && lhs.baseMipLevel < rhs.baseMipLevel
                + std::min(rhs.levelCount, UINT32_MAX - rhs.baseMipLevel)
        && rhs.baseMipLevel < lhs.baseMipLevel
                + std::min(lhs.levelCount, UINT32_MAX - lhs.baseMipLevel)
        && lhs.baseArrayLayer < rhs.baseArrayLayer
                + std::min(rhs.layerCount, UINT32_MAX - rhs.baseArrayLayer)
        && rhs.baseArrayLayer < lhs.baseArrayLayer
                + std::min(lhs.layerCount, UINT32_MAX - lhs.baseArrayLayer);
}

bool RenderGraph::overlaps(
    const BufferRange& lhs,
    const BufferRange& rhs) noexcept
{
    const VkDeviceSize lhsEnd = lhs.offset
        + std::min(lhs.size, std::numeric_limits<VkDeviceSize>::max() - lhs.offset);
    const VkDeviceSize rhsEnd = rhs.offset
        + std::min(rhs.size, std::numeric_limits<VkDeviceSize>::max() - rhs.offset);
    return lhs.offset < rhsEnd && rhs.offset < lhsEnd;
}

void RenderGraph::addImageUse(
    size_t passIndex,
    ImageHandle resource,
    ImageUse use,
    ImageSubresourceRange range)
{
    if (passIndex >= m_passes.size()) {
        throw std::out_of_range("RenderGraph pass index out of range");
    }
    validateHandle(resourceHandle(resource), "add image use");
    const ImageDesc& desc = std::get<ImageDesc>(
        m_resources[resource.index].description);
    range = normalizeImageRange(desc, range);
    const UseSemantics semantics = imageUseSemantics(use);
    if ((desc.usage & semantics.imageUsage) != semantics.imageUsage) {
        throw std::runtime_error(
            "RenderGraph image use is not supported by descriptor usage: "
            + m_resources[resource.index].name);
    }
    if (use == ImageUse::ColorAttachment
        && (range.aspect & ~VK_IMAGE_ASPECT_COLOR_BIT) != 0) {
        throw std::runtime_error(
            "RenderGraph color attachment use requires color aspect: "
            + m_resources[resource.index].name);
    }
    if (use == ImageUse::DepthAttachment
        && (range.aspect
            & (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT)) == 0) {
        throw std::runtime_error(
            "RenderGraph depth attachment use requires depth/stencil aspect: "
            + m_resources[resource.index].name);
    }
    m_passes[passIndex].uses.push_back({
        resourceHandle(resource), semantics.state, range, {}, use,
        BufferUse::FragmentStorageRead, semantics.reads, semantics.writes,
        false, SIZE_MAX});
}

void RenderGraph::addBufferUse(
    size_t passIndex,
    BufferHandle resource,
    BufferUse use,
    BufferRange range)
{
    if (passIndex >= m_passes.size()) {
        throw std::out_of_range("RenderGraph pass index out of range");
    }
    validateHandle(resourceHandle(resource), "add buffer use");
    const BufferDesc& desc = std::get<BufferDesc>(
        m_resources[resource.index].description);
    range = normalizeBufferRange(desc, range);
    const UseSemantics semantics = bufferUseSemantics(use);
    if ((desc.usage & semantics.bufferUsage) != semantics.bufferUsage) {
        throw std::runtime_error(
            "RenderGraph buffer use is not supported by descriptor usage: "
            + m_resources[resource.index].name);
    }
    m_passes[passIndex].uses.push_back({
        resourceHandle(resource), semantics.state, {}, range,
        ImageUse::FragmentSampled, use, semantics.reads, semantics.writes,
        false, SIZE_MAX});
}

void RenderGraph::addAttachment(
    size_t passIndex,
    ImageHandle resource,
    AttachmentType type,
    AttachmentLoadPolicy loadPolicy,
    AttachmentStorePolicy storePolicy)
{
    if (passIndex >= m_passes.size()) {
        throw std::out_of_range("RenderGraph pass index out of range");
    }
    validateHandle(resourceHandle(resource), "add attachment");
    auto& attachments = m_passes[passIndex].attachments;
    if (std::any_of(
            attachments.begin(), attachments.end(),
            [&](const PassAttachment& current) {
                return current.resource == resource;
            })) {
        throw std::runtime_error(
            "RenderGraph pass declares an attachment more than once");
    }
    if (type == AttachmentType::Depth
        && std::any_of(
            attachments.begin(), attachments.end(),
            [](const PassAttachment& current) {
                return current.type == AttachmentType::Depth;
            })) {
        throw std::runtime_error(
            "RenderGraph pass supports at most one depth attachment");
    }
    attachments.push_back({resource, type, loadPolicy, storePolicy});
    const size_t attachmentIndex = attachments.size() - 1;
    addImageUse(
        passIndex,
        resource,
        type == AttachmentType::Depth
            ? ImageUse::DepthAttachment
            : ImageUse::ColorAttachment,
        {});
    CompiledResourceUse& use = m_passes[passIndex].uses.back();
    use.attachment = true;
    use.attachmentIndex = attachmentIndex;
    use.reads = loadPolicy == AttachmentLoadPolicy::Load;
    use.writes = true;
    if (!use.reads) {
        use.state.access &= type == AttachmentType::Depth
            ? ~VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT
            : ~VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT;
    }
}

void RenderGraph::markExported(ResourceHandle resource)
{
    validateHandle(resource, "export resource");
    m_resources[resource.index].exported = true;
}

void RenderGraph::addExplicitDependency(
    size_t passIndex,
    std::string_view passName)
{
    if (passIndex >= m_passes.size()) {
        throw std::out_of_range("RenderGraph pass index out of range");
    }
    if (passName.empty()) {
        throw std::invalid_argument(
            "RenderGraph explicit dependency name cannot be empty");
    }
    auto& dependencies = m_passes[passIndex].explicitDependencies;
    if (std::find(dependencies.begin(), dependencies.end(), passName)
        == dependencies.end()) {
        dependencies.emplace_back(passName);
    }
}

void RenderGraph::validateHandle(
    ResourceHandle handle,
    std::string_view operation) const
{
    if (!handle.isValid()) {
        throw std::invalid_argument(
            "RenderGraph " + std::string(operation) + ": invalid handle");
    }
    if (handle.graphGeneration != m_generation) {
        throw std::invalid_argument(
            "RenderGraph " + std::string(operation) + ": stale handle generation");
    }
    if (handle.index >= m_resources.size()) {
        throw std::invalid_argument(
            "RenderGraph " + std::string(operation) + ": handle index out of range");
    }
    if (m_resources[handle.index].kind != handle.kind) {
        throw std::invalid_argument(
            "RenderGraph " + std::string(operation) + ": wrong resource kind");
    }
}

void RenderGraph::validateResourcesAndUses() const
{
    std::vector<std::map<size_t, uint8_t>> usage(m_resources.size());
    for (size_t passIndex = 0; passIndex < m_passes.size(); ++passIndex) {
        const PassNode& pass = m_passes[passIndex];
        for (size_t useIndex = 0; useIndex < pass.uses.size(); ++useIndex) {
            const CompiledResourceUse& use = pass.uses[useIndex];
            validateHandle(use.resource, "validate use");
            if (use.state.stages == VK_PIPELINE_STAGE_2_NONE
                || use.state.access == VK_ACCESS_2_NONE) {
                throw std::runtime_error(
                    "RenderGraph use has empty stage/access: pass '"
                    + pass.name + "', resource '"
                    + m_resources[use.resource.index].name + "'");
            }
            if (use.resource.kind == ResourceKind::Buffer
                && use.state.layout != VK_IMAGE_LAYOUT_UNDEFINED) {
                throw std::runtime_error(
                    "RenderGraph buffer use cannot declare an image layout");
            }
            uint8_t& mode = usage[use.resource.index][passIndex];
            mode |= use.reads ? kReadMask : 0;
            mode |= use.writes ? kWriteMask : 0;

            for (size_t previousIndex = 0; previousIndex < useIndex;
                 ++previousIndex) {
                const CompiledResourceUse& previous = pass.uses[previousIndex];
                if (previous.resource != use.resource) continue;
                const bool rangeOverlap = use.resource.kind == ResourceKind::Image
                    ? overlaps(previous.imageRange, use.imageRange)
                    : overlaps(previous.bufferRange, use.bufferRange);
                if (rangeOverlap && (previous.writes || use.writes)) {
                    throw std::runtime_error(
                        "RenderGraph same-pass overlapping read/write is unsupported: pass '"
                        + pass.name + "', resource '"
                        + m_resources[use.resource.index].name + "'");
                }
                if (rangeOverlap
                    && use.resource.kind == ResourceKind::Image
                    && previous.state.layout != use.state.layout) {
                    throw std::runtime_error(
                        "RenderGraph same-pass overlapping image uses require one layout: pass '"
                        + pass.name + "', resource '"
                        + m_resources[use.resource.index].name + "'");
                }
            }
        }
    }

    for (size_t index = 0; index < m_resources.size(); ++index) {
        const ResourceDesc& resourceDesc = m_resources[index];
        if (resourceDesc.kind == ResourceKind::Image) {
            const ImageDesc& desc = std::get<ImageDesc>(resourceDesc.description);
            if (desc.format == VK_FORMAT_UNDEFINED || desc.usage == 0
                || desc.aspect == 0) {
                throw std::runtime_error(
                    "RenderGraph image description is incomplete: "
                    + resourceDesc.name);
            }
            if (desc.mipLevels != 1 || desc.arrayLayers != 1
                || desc.samples != VK_SAMPLE_COUNT_1_BIT) {
                throw std::runtime_error(
                    "RenderGraph WP01 supports only 2D mip1 layer1 sample1 images: "
                    + resourceDesc.name);
            }
            if (desc.extent.mode == ImageExtentMode::Absolute
                && (desc.extent.absolute.width == 0
                    || desc.extent.absolute.height == 0)) {
                throw std::runtime_error(
                    "RenderGraph absolute image extent must be non-zero: "
                    + resourceDesc.name);
            }
            if (desc.extent.mode == ImageExtentMode::SwapchainRelative
                && (desc.extent.absolute.width != 0
                    || desc.extent.absolute.height != 0)) {
                throw std::runtime_error(
                    "RenderGraph relative image extent cannot include absolute dimensions: "
                    + resourceDesc.name);
            }
            if (resourceDesc.ownership == ResourceOwnership::Internal
                && desc.initialContent == InitialContent::Preserved) {
                throw std::runtime_error(
                    "RenderGraph Preserved initial content is external-only: "
                    + resourceDesc.name);
            }
        } else {
            const BufferDesc& desc = std::get<BufferDesc>(resourceDesc.description);
            if (desc.size == 0 || desc.usage == 0) {
                throw std::runtime_error(
                    "RenderGraph buffer description is incomplete: "
                    + resourceDesc.name);
            }
            if (resourceDesc.ownership == ResourceOwnership::Internal
                && desc.initialContent == InitialContent::Preserved) {
                throw std::runtime_error(
                    "RenderGraph Preserved initial content is external-only: "
                    + resourceDesc.name);
            }
            if (resourceDesc.ownership == ResourceOwnership::Internal
                && desc.initialContent == InitialContent::Cleared) {
                throw std::runtime_error(
                    "RenderGraph WP01 cannot implicitly clear internal buffers: "
                    + resourceDesc.name);
            }
        }

        if (resourceDesc.ownership == ResourceOwnership::Internal
            && usage[index].empty()) {
            throw std::runtime_error(
                "RenderGraph internal resource is unused: " + resourceDesc.name);
        }
        if (usage[index].empty()) continue;

        const auto first = usage[index].begin();
        const InitialContent initial = resourceDesc.kind == ResourceKind::Image
            ? std::get<ImageDesc>(resourceDesc.description).initialContent
            : std::get<BufferDesc>(resourceDesc.description).initialContent;
        if (initial == InitialContent::Undefined && hasRead(first->second)) {
            throw std::runtime_error(
                "RenderGraph resource with Undefined content is read first: "
                + resourceDesc.name);
        }
        if (resourceDesc.kind == ResourceKind::Image) {
            if (const PassAttachment* attachment =
                    findAttachment(m_passes[first->first], static_cast<uint32_t>(index));
                attachment != nullptr
                && attachment->loadPolicy == AttachmentLoadPolicy::Load
                && initial != InitialContent::Preserved) {
                throw std::runtime_error(
                    "RenderGraph attachment LOAD requires defined initial content: "
                    + resourceDesc.name);
            }
            if (resourceDesc.ownership == ResourceOwnership::Internal
                && initial == InitialContent::Cleared) {
                const PassAttachment* attachment = findAttachment(
                    m_passes[first->first], static_cast<uint32_t>(index));
                if (attachment == nullptr
                    || (attachment->loadPolicy != AttachmentLoadPolicy::Clear
                        && attachment->loadPolicy
                            != AttachmentLoadPolicy::RuntimeDefault)) {
                    throw std::runtime_error(
                        "RenderGraph Cleared internal image must first be a CLEAR attachment: "
                        + resourceDesc.name);
                }
            }
        }

        if (!resourceDesc.exported) continue;
        bool defined = initial == InitialContent::Preserved;
        for (const auto& [passIndex, mode] : usage[index]) {
            if (!hasWrite(mode)) continue;
            if (resourceDesc.kind == ResourceKind::Buffer) {
                defined = true;
                continue;
            }
            const PassAttachment* attachment =
                findAttachment(m_passes[passIndex], static_cast<uint32_t>(index));
            defined = attachment != nullptr
                && attachment->storePolicy == AttachmentStorePolicy::Store;
        }
        if (!defined) {
            throw std::runtime_error(
                "RenderGraph exported resource lacks defined stored contents: "
                + resourceDesc.name);
        }
    }

    for (const PassNode& pass : m_passes) {
        if (!pass.attachments.empty()) {
            if (pass.pass != nullptr
                && pass.pass->requiresOutsideRenderingScope()) {
                throw std::runtime_error(
                    "RenderGraph callback native transfer/compute commands cannot execute inside an attachment pass: "
                    + pass.name);
            }
            for (const CompiledResourceUse& use : pass.uses) {
                const bool transfer = use.resource.kind == ResourceKind::Image
                    ? use.imageUse == ImageUse::TransferSource
                        || use.imageUse == ImageUse::TransferDestination
                    : use.bufferUse == BufferUse::TransferSource
                        || use.bufferUse == BufferUse::TransferDestination;
                const bool compute =
                    (use.state.stages & VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT) != 0;
                if (transfer || compute) {
                    throw std::runtime_error(
                        "RenderGraph transfer/compute use cannot execute inside an attachment pass: "
                        + pass.name);
                }
            }
        }
        for (const PassAttachment& attachment : pass.attachments) {
            const ResourceDesc& resourceDesc =
                resource(resourceHandle(attachment.resource));
            const ImageDesc& desc = std::get<ImageDesc>(resourceDesc.description);
            const bool depthAspect =
                (desc.aspect
                    & (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT))
                != 0;
            if (attachment.type == AttachmentType::Depth) {
                if (!depthAspect
                    || (desc.usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) == 0) {
                    throw std::runtime_error(
                        "RenderGraph depth attachment description mismatch: "
                        + resourceDesc.name);
                }
            } else if (depthAspect
                || (desc.aspect & VK_IMAGE_ASPECT_COLOR_BIT) == 0
                || (desc.usage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) == 0) {
                throw std::runtime_error(
                    "RenderGraph color attachment description mismatch: "
                    + resourceDesc.name);
            }
        }
    }
}

std::optional<ImageHandle> RenderGraph::findImage(std::string_view name) const
{
    const auto found = std::find_if(
        m_resources.begin(), m_resources.end(),
        [name](const ResourceDesc& resource) {
            return resource.name == name && resource.kind == ResourceKind::Image;
        });
    if (found == m_resources.end()) return std::nullopt;
    return ImageHandle{
        static_cast<uint32_t>(std::distance(m_resources.begin(), found)),
        m_generation};
}

std::optional<BufferHandle> RenderGraph::findBuffer(std::string_view name) const
{
    const auto found = std::find_if(
        m_resources.begin(), m_resources.end(),
        [name](const ResourceDesc& resource) {
            return resource.name == name && resource.kind == ResourceKind::Buffer;
        });
    if (found == m_resources.end()) return std::nullopt;
    return BufferHandle{
        static_cast<uint32_t>(std::distance(m_resources.begin(), found)),
        m_generation};
}

const ResourceDesc& RenderGraph::resource(ResourceHandle handle) const
{
    validateHandle(handle, "resolve resource");
    return m_resources[handle.index];
}

const ResourceDesc& RenderGraph::resource(ImageHandle handle) const
{
    return resource(resourceHandle(handle));
}

const ResourceDesc& RenderGraph::resource(BufferHandle handle) const
{
    return resource(resourceHandle(handle));
}

} // namespace ku
