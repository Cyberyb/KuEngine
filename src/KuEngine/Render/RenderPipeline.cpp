#include "RenderPipeline.h"

#include "RenderContext.h"
#include <KuEngine/Core/Log.h>
#include <KuEngine/RHI/CommandList.h>
#include <KuEngine/RHI/RHIDevice.h>

#include <imgui.h>

#include <algorithm>
#include <sstream>
#include <stdexcept>

namespace ku {
namespace {

const char* toString(ResourceHazardType hazard)
{
    switch (hazard) {
        case ResourceHazardType::ReadAfterWrite: return "RAW";
        case ResourceHazardType::WriteAfterRead: return "WAR";
        case ResourceHazardType::WriteAfterWrite: return "WAW";
    }
    return "?";
}

const char* toString(VkImageLayout layout)
{
    switch (layout) {
        case VK_IMAGE_LAYOUT_UNDEFINED: return "UNDEFINED";
        case VK_IMAGE_LAYOUT_GENERAL: return "GENERAL";
        case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
            return "COLOR_ATTACHMENT_OPTIMAL";
        case VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL:
            return "DEPTH_ATTACHMENT_OPTIMAL";
        case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
            return "SHADER_READ_ONLY_OPTIMAL";
        case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL: return "TRANSFER_SRC_OPTIMAL";
        case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL: return "TRANSFER_DST_OPTIMAL";
        case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR: return "PRESENT_SRC_KHR";
        default: return "OTHER";
    }
}

VkAttachmentLoadOp resolveLoadOp(
    AttachmentLoadPolicy policy,
    VkAttachmentLoadOp runtimeDefault)
{
    switch (policy) {
        case AttachmentLoadPolicy::RuntimeDefault: return runtimeDefault;
        case AttachmentLoadPolicy::Load: return VK_ATTACHMENT_LOAD_OP_LOAD;
        case AttachmentLoadPolicy::Clear: return VK_ATTACHMENT_LOAD_OP_CLEAR;
        case AttachmentLoadPolicy::DontCare: return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    }
    return runtimeDefault;
}

VkAttachmentStoreOp resolveStoreOp(
    AttachmentStorePolicy policy,
    VkAttachmentStoreOp runtimeDefault)
{
    switch (policy) {
        case AttachmentStorePolicy::RuntimeDefault: return runtimeDefault;
        case AttachmentStorePolicy::Store: return VK_ATTACHMENT_STORE_OP_STORE;
        case AttachmentStorePolicy::DontCare: return VK_ATTACHMENT_STORE_OP_DONT_CARE;
    }
    return runtimeDefault;
}

} // namespace

RenderPipeline::RenderPipeline()
    : m_exportLifetime(std::make_shared<ExportLifetimeState>())
{
}

RenderPipeline::~RenderPipeline()
{
    if (m_exportLifetime) {
        m_exportLifetime->alive = false;
        ++m_exportLifetime->epoch;
    }
    KU_INFO("RenderPipeline destroyed ({} passes)", m_passes.size());
}

void RenderPipeline::compile(const RenderContext& context)
{
    m_compiled = false;
    invalidateExports();

    RenderGraph candidateGraph;
    for (auto& pass : m_passes) {
        pass->initialize(context);
        const size_t graphIndex = candidateGraph.registerPass(*pass);
        auto builder = candidateGraph.buildPass(graphIndex);
        pass->setup(builder);
    }
    candidateGraph.compile();

    RHIRenderGraphResourceAllocator allocator(context.device);
    RenderGraphResourcePool candidatePool = RenderGraphResourcePool::build(
        candidateGraph,
        context.initialExtent,
        m_nextAllocationGeneration,
        allocator);

    for (auto& pass : m_passes) {
        pass->prepare(context);
    }

    std::vector<size_t> candidateOrder;
    const auto& graphPasses = candidateGraph.passes();
    const auto& resources = candidateGraph.resources();
    for (const size_t passIndex : candidateGraph.executionOrder()) {
        if (passIndex >= graphPasses.size()) continue;
        const PassNode& node = graphPasses[passIndex];
        if (node.pass) candidateOrder.push_back(passIndex);
        std::ostringstream accessSummary;
        for (size_t i = 0; i < node.uses.size(); ++i) {
            if (i) accessSummary << ", ";
            const auto& use = node.uses[i];
            accessSummary << (use.reads ? "R" : "")
                << (use.writes ? "W" : "") << ":"
                << resources[use.resource.index].name;
        }
        KU_INFO(
            "RenderPass compiled: {} (declared resources: {})",
            node.name,
            accessSummary.str().empty() ? "none" : accessSummary.str());
    }

    std::ostringstream orderSummary;
    for (size_t i = 0; i < candidateGraph.executionOrder().size(); ++i) {
        if (i) orderSummary << " -> ";
        orderSummary << graphPasses[candidateGraph.executionOrder()[i]].name;
    }
    for (const PassDependencyEdge& dependency : candidateGraph.dependencies()) {
        KU_INFO(
            "RenderGraph dependency: {} -> {} ({})",
            graphPasses[dependency.fromPass].name,
            graphPasses[dependency.toPass].name,
            dependency.explicitDependency ? "explicit" : "resource");
    }
    for (const BarrierPlanItem& barrier : candidateGraph.barrierPlan()) {
        KU_INFO(
            "RenderGraph barrier-plan: {} -> {} | {} ({})",
            graphPasses[barrier.fromPass].name,
            graphPasses[barrier.toPass].name,
            resources[barrier.resource.index].name,
            toString(barrier.hazard));
    }

    CompileDebugInfo candidateDebug{};
    candidateDebug.passCount = graphPasses.size();
    candidateDebug.resourceCount = resources.size();
    candidateDebug.dependencyCount = candidateGraph.dependencies().size();
    candidateDebug.barrierCount = candidateGraph.barrierPlan().size();
    candidateDebug.orderSummary = orderSummary.str();

    m_renderGraph = std::move(candidateGraph);
    m_resourcePool.swap(candidatePool);
    m_compiledExecutionOrder = std::move(candidateOrder);
    m_compileDebug = std::move(candidateDebug);
    m_externalImageBindings.clear();
    m_externalBufferBindings.clear();
    m_device = &context.device;
    ++m_nextAllocationGeneration;
    m_executeCompleted = false;
    m_compiled = true;

    KU_INFO(
        "RenderGraph executable compile finished (passes={}, resources={}, dependencies={}, barriers={}, order={})",
        m_compileDebug.passCount,
        m_compileDebug.resourceCount,
        m_compileDebug.dependencyCount,
        m_compileDebug.barrierCount,
        m_compileDebug.orderSummary.empty() ? "none" : m_compileDebug.orderSummary);
}

void RenderPipeline::beginFrame()
{
    ++m_exportLifetime->frameSerial;
    m_executeCompleted = false;
    m_resourcePool.invalidateTransientContents();
}

void RenderPipeline::execute(CommandList& cmd, const FrameData& frame)
{
    if (!m_compiled) {
        throw std::runtime_error(
            "RenderPipeline must be compiled before graph execution");
    }
    beginFrame();
    m_executeDebug = {};
    m_executeDebug.frameIndex = frame.frameIndex;
    m_barrierDebugEvents.clear();

    const auto& graphPasses = m_renderGraph.passes();
    const auto& resources = m_renderGraph.resources();
    std::vector<StatePlannerResource> plannerResources;
    plannerResources.reserve(resources.size());
    m_executionResourceStates.clear();
    m_executionResourceStates.reserve(resources.size());
    for (uint32_t index = 0; index < resources.size(); ++index) {
        const ResourceDesc& resource = resources[index];
        const ResourceHandle handle{
            index, m_renderGraph.generation(), resource.kind};
        if (resource.kind == ResourceKind::Image) {
            const ImageDesc& desc = std::get<ImageDesc>(resource.description);
            const ResolvedImage image = resolveImagePhysical(
                ImageHandle{index, m_renderGraph.generation()});
            plannerResources.push_back({
                handle, resource.name, image.state, image.contentsValid,
                RenderGraph::normalizeImageRange(desc, {}), {}});
            m_executionResourceStates.push_back(
                {handle, image.state, image.contentsValid});
        } else {
            const BufferDesc& desc = std::get<BufferDesc>(resource.description);
            const ResolvedBuffer buffer = resolveBufferPhysical(
                BufferHandle{index, m_renderGraph.generation()});
            plannerResources.push_back({
                handle, resource.name, buffer.state, buffer.contentsValid,
                {}, RenderGraph::normalizeBufferRange(desc, {})});
            m_executionResourceStates.push_back(
                {handle, buffer.state, buffer.contentsValid});
        }
    }

    std::vector<StatePlannerPass> plannerPasses;
    plannerPasses.reserve(m_compiledExecutionOrder.size());
    for (const size_t passIndex : m_compiledExecutionOrder) {
        if (passIndex >= graphPasses.size()) continue;
        const PassNode& node = graphPasses[passIndex];
        StatePlannerPass plannedPass{};
        plannedPass.graphPassIndex = passIndex;
        plannedPass.name = node.name;
        plannedPass.enabled = node.pass && node.pass->enabled();
        plannedPass.uses.reserve(node.uses.size());
        for (const CompiledResourceUse& declared : node.uses) {
            StatePlannerUse runtimeUse{declared, false};
            if (declared.attachment) {
                const PassAttachment& attachment =
                    node.attachments.at(declared.attachmentIndex);
                const ResourceDesc& logical =
                    resources[attachment.resource.index];
                VkAttachmentLoadOp defaultLoad = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
                VkAttachmentStoreOp defaultStore = VK_ATTACHMENT_STORE_OP_STORE;
                if (logical.external()) {
                    const ExternalImageBinding& binding =
                        m_externalImageBindings.at(attachment.resource.index);
                    defaultLoad = binding.defaultLoadOp;
                    defaultStore = binding.defaultStoreOp;
                } else if (std::get<ImageDesc>(logical.description).initialContent
                    == InitialContent::Cleared) {
                    defaultLoad = VK_ATTACHMENT_LOAD_OP_CLEAR;
                }
                const VkAttachmentLoadOp load =
                    resolveLoadOp(attachment.loadPolicy, defaultLoad);
                const VkAttachmentStoreOp store =
                    resolveStoreOp(attachment.storePolicy, defaultStore);
                runtimeUse.use.reads = load == VK_ATTACHMENT_LOAD_OP_LOAD;
                runtimeUse.use.writes = true;
                const VkAccessFlags2 readBit = attachment.type == AttachmentType::Depth
                    ? VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT
                    : VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT;
                if (runtimeUse.use.reads) runtimeUse.use.state.access |= readBit;
                else runtimeUse.use.state.access &= ~readBit;
                runtimeUse.discardAfter = store != VK_ATTACHMENT_STORE_OP_STORE;
            }
            plannedPass.uses.push_back(runtimeUse);
        }
        plannerPasses.push_back(std::move(plannedPass));
    }

    // Complete, side-effect-free preflight before the first graph command.
    const RenderGraphStatePlan plan =
        RenderGraphStatePlanner::plan(plannerResources, plannerPasses);

    try {
        for (size_t planPassIndex = 0;
             planPassIndex < plannerPasses.size();
             ++planPassIndex) {
            const StatePlannerPass& plannedPass = plannerPasses[planPassIndex];
            const PassNode& node = graphPasses[plannedPass.graphPassIndex];
            if (!plannedPass.enabled) continue;

            std::vector<VkImageMemoryBarrier2> imageBarriers;
            std::vector<VkBufferMemoryBarrier2> bufferBarriers;
            for (const PlannedResourceBarrier& barrier
                 : plan.barriersBeforePass[planPassIndex]) {
                ++m_executeDebug.plannedBarriers;
                if (barrier.resource.kind == ResourceKind::Image) {
                    const ResolvedImage image = resolveImagePhysical({
                        barrier.resource.index,
                        barrier.resource.graphGeneration});
                    VkImageMemoryBarrier2 native{};
                    native.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
                    native.srcStageMask = barrier.before.stages;
                    native.srcAccessMask = barrier.before.access;
                    native.dstStageMask = barrier.after.stages;
                    native.dstAccessMask = barrier.after.access;
                    native.oldLayout = barrier.before.layout;
                    native.newLayout = barrier.after.layout;
                    native.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                    native.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                    native.image = image.image;
                    native.subresourceRange = {
                        barrier.imageRange.aspect,
                        barrier.imageRange.baseMipLevel,
                        barrier.imageRange.levelCount,
                        barrier.imageRange.baseArrayLayer,
                        barrier.imageRange.layerCount};
                    imageBarriers.push_back(native);
                } else {
                    const ResolvedBuffer buffer = resolveBufferPhysical({
                        barrier.resource.index,
                        barrier.resource.graphGeneration});
                    VkBufferMemoryBarrier2 native{};
                    native.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
                    native.srcStageMask = barrier.before.stages;
                    native.srcAccessMask = barrier.before.access;
                    native.dstStageMask = barrier.after.stages;
                    native.dstAccessMask = barrier.after.access;
                    native.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                    native.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                    native.buffer = buffer.buffer;
                    native.offset = barrier.bufferRange.offset;
                    native.size = barrier.bufferRange.size;
                    bufferBarriers.push_back(native);
                }
                m_executionResourceStates[barrier.resource.index].state =
                    barrier.after;
                ++m_executeDebug.appliedBarriers;
            }
            if (!imageBarriers.empty() || !bufferBarriers.empty()) {
                cmd.pipelineBarrier2(imageBarriers, bufferBarriers);
            }
            for (const StatePlannerUse& use : plannedPass.uses) {
                m_executionResourceStates[use.use.resource.index].state =
                    use.use.state;
            }
            executePassNode(cmd, frame, node, resources);
            for (const StatePlannerUse& use : plannedPass.uses) {
                auto& shadow =
                    m_executionResourceStates[use.use.resource.index];
                if (use.use.writes) shadow.contentsValid = true;
                if (use.discardAfter) shadow.contentsValid = false;
            }
        }
    } catch (...) {
        m_currentPass = nullptr;
        m_executionResourceStates.clear();
        throw;
    }

    for (const PlannedResourceFinalState& final : plan.finalResources) {
        if (final.resource.kind == ResourceKind::Image) {
            setResolvedImageState(
                {final.resource.index, final.resource.graphGeneration},
                final.state,
                final.contentsValid);
        } else {
            setResolvedBufferState(
                {final.resource.index, final.resource.graphGeneration},
                final.state,
                final.contentsValid);
        }
    }
    m_executionResourceStates.clear();
    m_executeCompleted = true;
}

void RenderPipeline::executePassNode(
    CommandList& cmd,
    const FrameData& frame,
    const PassNode& node,
    const std::vector<ResourceDesc>& resources)
{
    if (node.attachments.empty()) {
        m_currentPass = &node;
        try { node.pass->execute(cmd, frame, *this); }
        catch (...) { m_currentPass = nullptr; throw; }
        m_currentPass = nullptr;
        return;
    }

    std::vector<VkRenderingAttachmentInfo> colorAttachments;
    VkRenderingAttachmentInfo depthAttachment{};
    bool hasDepthAttachment = false;
    VkExtent2D renderExtent{0, 0};
    for (const PassAttachment& attachment : node.attachments) {
        const ResourceDesc& logical = resources[attachment.resource.index];
        ResolvedImage image = resolveImagePhysical(attachment.resource);
        if (!image.complete()) {
            throw std::runtime_error(
                "RenderGraph attachment binding is incomplete: " + logical.name);
        }
        const bool depthAspect =
            (image.aspect
                & (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT)) != 0;
        if ((attachment.type == AttachmentType::Depth) != depthAspect) {
            throw std::runtime_error(
                "RenderGraph attachment type does not match image aspect: "
                + logical.name);
        }
        if (renderExtent.width == 0) {
            renderExtent = image.extent;
        } else if (renderExtent.width != image.extent.width
            || renderExtent.height != image.extent.height) {
            throw std::runtime_error(
                "RenderGraph pass attachments must have matching extents");
        }

        VkAttachmentLoadOp defaultLoad = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        VkAttachmentStoreOp defaultStore = VK_ATTACHMENT_STORE_OP_STORE;
        if (logical.external()) {
            const ExternalImageBinding& binding =
                m_externalImageBindings.at(attachment.resource.index);
            defaultLoad = binding.defaultLoadOp;
            defaultStore = binding.defaultStoreOp;
        } else if (std::get<ImageDesc>(logical.description).initialContent
            == InitialContent::Cleared) {
            defaultLoad = VK_ATTACHMENT_LOAD_OP_CLEAR;
        }
        const VkAttachmentLoadOp loadOp =
            resolveLoadOp(attachment.loadPolicy, defaultLoad);
        const VkAttachmentStoreOp storeOp =
            resolveStoreOp(attachment.storePolicy, defaultStore);
        VkRenderingAttachmentInfo info{};
        info.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        info.imageView = image.imageView;
        info.imageLayout = image.currentLayout;
        info.loadOp = loadOp;
        info.storeOp = storeOp;
        info.clearValue = image.clearValue;
        if (attachment.type == AttachmentType::Depth) {
            depthAttachment = info;
            hasDepthAttachment = true;
        } else {
            colorAttachments.push_back(info);
        }
    }

    VkRenderingInfo renderingInfo{};
    renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderingInfo.renderArea.extent = renderExtent;
    renderingInfo.layerCount = 1;
    renderingInfo.colorAttachmentCount =
        static_cast<uint32_t>(colorAttachments.size());
    renderingInfo.pColorAttachments =
        colorAttachments.empty() ? nullptr : colorAttachments.data();
    renderingInfo.pDepthAttachment =
        hasDepthAttachment ? &depthAttachment : nullptr;
    vkCmdBeginRendering(cmd, &renderingInfo);
    ++m_executeDebug.renderingScopes;

    const ViewerPixelRect sceneRect = fitViewerRectToExtent(
        frame.viewerLayout.sceneFramebuffer,
        {renderExtent.width, renderExtent.height});
    VkViewport viewport{};
    viewport.x = static_cast<float>(sceneRect.x);
    viewport.y = static_cast<float>(sceneRect.y);
    viewport.width = static_cast<float>(sceneRect.width);
    viewport.height = static_cast<float>(sceneRect.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(cmd, 0, 1, &viewport);
    VkRect2D scissor{};
    scissor.offset = {
        static_cast<int32_t>(sceneRect.x),
        static_cast<int32_t>(sceneRect.y)};
    scissor.extent = {sceneRect.width, sceneRect.height};
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    try {
        m_currentPass = &node;
        node.pass->execute(cmd, frame, *this);
    } catch (...) {
        m_currentPass = nullptr;
        vkCmdEndRendering(cmd);
        throw;
    }
    m_currentPass = nullptr;
    vkCmdEndRendering(cmd);
}

void RenderPipeline::update(const FrameData& frame)
{
    for (const auto& pass : m_passes) {
        if (pass && pass->enabled()) pass->update(frame);
    }
}

void RenderPipeline::bindExternalImage(
    ImageHandle handle,
    const ExternalImageBindingInfo& info)
{
    if (!m_compiled) {
        throw std::runtime_error("External image binding requires a compiled graph");
    }
    const ResourceDesc& logical = m_renderGraph.resource(handle);
    if (!logical.external() || logical.kind != ResourceKind::Image) {
        throw std::invalid_argument(
            "External image binding handle is not an imported image");
    }
    const ImageDesc& expected = std::get<ImageDesc>(logical.description);
    validateExternalImageBindingInfo(
        expected,
        m_resourcePool.swapchainExtent(),
        info,
        logical.name);
    m_externalImageBindings[handle.index] = ExternalImageBinding{
        handle,
        info.image,
        info.imageView,
        info.extent,
        info.format,
        info.usage,
        info.initialState,
        info.aspect,
        info.clearValue,
        info.defaultLoadOp,
        info.defaultStoreOp,
        info.finalState,
        m_nextExternalBindingGeneration++,
        info.contentsValid,
    };
}

void RenderPipeline::bindExternalImage(const ExternalImageBindingInfo& info)
{
    if (info.resourceName.empty()) {
        throw std::invalid_argument("External image binding requires a name");
    }
    const auto handle = m_renderGraph.findImage(info.resourceName);
    if (!handle) {
        throw std::invalid_argument(
            "External image binding name is not declared: "
            + std::string(info.resourceName));
    }
    bindExternalImage(*handle, info);
}

void RenderPipeline::bindExternalBuffer(
    BufferHandle handle,
    const ExternalBufferBindingInfo& info)
{
    if (!m_compiled) {
        throw std::runtime_error("External buffer binding requires a compiled graph");
    }
    const ResourceDesc& logical = m_renderGraph.resource(handle);
    if (!logical.external() || logical.kind != ResourceKind::Buffer) {
        throw std::invalid_argument(
            "External buffer binding handle is not an imported buffer");
    }
    const BufferDesc& expected = std::get<BufferDesc>(logical.description);
    validateExternalBufferBindingInfo(expected, info, logical.name);
    m_externalBufferBindings[handle.index] = ExternalBufferBinding{
        handle,
        info.buffer,
        info.size,
        info.usage,
        info.initialState,
        info.finalState,
        m_nextExternalBindingGeneration++,
        info.contentsValid,
    };
}

void RenderPipeline::validateExternalImageBindingInfo(
    const ImageDesc& expected,
    VkExtent2D swapchainExtent,
    const ExternalImageBindingInfo& info,
    std::string_view logicalName)
{
    if (info.image == VK_NULL_HANDLE || info.imageView == VK_NULL_HANDLE
        || info.extent.width == 0 || info.extent.height == 0
        || info.format == VK_FORMAT_UNDEFINED || info.aspect == 0) {
        throw std::invalid_argument("External image binding is incomplete");
    }
    const VkExtent2D expectedExtent =
        resolveImageExtent(expected, swapchainExtent);
    if (info.format != expected.format
        || info.extent.width != expectedExtent.width
        || info.extent.height != expectedExtent.height
        || info.aspect != expected.aspect) {
        throw std::invalid_argument(
            "External image binding does not match declared format/extent/aspect");
    }
    if (info.usage == 0
        || (info.usage & expected.usage) != expected.usage) {
        const std::string_view name = logicalName.empty()
            ? info.resourceName
            : logicalName;
        std::ostringstream message;
        message << "External image binding usage mismatch for resource '"
            << (name.empty() ? "<unnamed>" : name)
            << "': expected=" << expected.usage
            << " actual=" << info.usage;
        throw std::invalid_argument(message.str());
    }
    if (expected.initialContent == InitialContent::Preserved
        && !info.contentsValid) {
        throw std::invalid_argument(
            "External image declared Preserved requires valid bound contents");
    }
}

void RenderPipeline::validateExternalBufferBindingInfo(
    const BufferDesc& expected,
    const ExternalBufferBindingInfo& info,
    std::string_view logicalName)
{
    if (info.buffer == VK_NULL_HANDLE || info.size == 0) {
        throw std::invalid_argument("External buffer binding is incomplete");
    }
    if (info.size < expected.size) {
        throw std::invalid_argument(
            "External buffer binding does not match declared size");
    }
    if (info.usage == 0
        || (info.usage & expected.usage) != expected.usage) {
        const std::string_view name = logicalName.empty()
            ? info.resourceName
            : logicalName;
        std::ostringstream message;
        message << "External buffer binding usage mismatch for resource '"
            << (name.empty() ? "<unnamed>" : name)
            << "': expected=" << expected.usage
            << " actual=" << info.usage;
        throw std::invalid_argument(message.str());
    }
    if (expected.initialContent == InitialContent::Preserved
        && !info.contentsValid) {
        throw std::invalid_argument(
            "External buffer declared Preserved requires valid bound contents");
    }
}

void RenderPipeline::bindExternalBuffer(const ExternalBufferBindingInfo& info)
{
    if (info.resourceName.empty()) {
        throw std::invalid_argument("External buffer binding requires a name");
    }
    const auto handle = m_renderGraph.findBuffer(info.resourceName);
    if (!handle) {
        throw std::invalid_argument(
            "External buffer binding name is not declared: "
            + std::string(info.resourceName));
    }
    bindExternalBuffer(*handle, info);
}

bool RenderPipeline::currentPassDeclares(
    ResourceHandle resource,
    ImageUse use,
    ImageSubresourceRange range) const
{
    return m_currentPass != nullptr
        && passDeclaresUse(*m_currentPass, resource, use, range);
}

bool RenderPipeline::currentPassDeclares(
    ResourceHandle resource,
    BufferUse use,
    BufferRange range) const
{
    return m_currentPass != nullptr
        && passDeclaresUse(*m_currentPass, resource, use, range);
}

ResolvedImage RenderPipeline::resolveImage(
    ImageHandle handle,
    ImageUse declaredUse,
    ImageSubresourceRange range)
{
    const ResourceDesc& logical = m_renderGraph.resource(handle);
    range = RenderGraph::normalizeImageRange(
        std::get<ImageDesc>(logical.description), range);
    if (!currentPassDeclares(resourceHandle(handle), declaredUse, range)) {
        throw std::runtime_error(
            "RenderGraph pass attempted undeclared image use or range access");
    }
    return resolveImagePhysical(handle);
}

ResolvedBuffer RenderPipeline::resolveBuffer(
    BufferHandle handle,
    BufferUse declaredUse,
    BufferRange range)
{
    const ResourceDesc& logical = m_renderGraph.resource(handle);
    range = RenderGraph::normalizeBufferRange(
        std::get<BufferDesc>(logical.description), range);
    if (!currentPassDeclares(resourceHandle(handle), declaredUse, range)) {
        throw std::runtime_error(
            "RenderGraph pass attempted undeclared buffer use or range access");
    }
    return resolveBufferPhysical(handle);
}

ResolvedImage RenderPipeline::resolveImagePhysical(ImageHandle handle)
{
    const ResourceDesc& logical = m_renderGraph.resource(handle);
    ResolvedImage resolved{};
    if (!logical.external()) {
        resolved = m_resourcePool.resolveImage(handle);
    } else {
    const auto found = m_externalImageBindings.find(handle.index);
    if (found == m_externalImageBindings.end()) {
        throw std::runtime_error(
            "RenderGraph external image is not bound: " + logical.name);
    }
    const ExternalImageBinding& binding = found->second;
    resolved = ResolvedImage{
        handle,
        binding.image,
        binding.imageView,
        binding.extent,
        binding.format,
        binding.usage,
        binding.aspect,
        binding.state,
        binding.state.layout,
        binding.clearValue,
        binding.allocationGeneration,
        binding.contentsValid,
        true,
    };
    }
    if (handle.index < m_executionResourceStates.size()) {
        const auto& shadow = m_executionResourceStates[handle.index];
        if (shadow.resource == resourceHandle(handle)) {
            resolved.state = shadow.state;
            resolved.currentLayout = shadow.state.layout;
            resolved.contentsValid = shadow.contentsValid;
        }
    }
    return resolved;
}

ResolvedBuffer RenderPipeline::resolveBufferPhysical(BufferHandle handle)
{
    const ResourceDesc& logical = m_renderGraph.resource(handle);
    ResolvedBuffer resolved{};
    if (!logical.external()) {
        resolved = m_resourcePool.resolveBuffer(handle);
    } else {
    const auto found = m_externalBufferBindings.find(handle.index);
    if (found == m_externalBufferBindings.end()) {
        throw std::runtime_error(
            "RenderGraph external buffer is not bound: " + logical.name);
    }
    const ExternalBufferBinding& binding = found->second;
    resolved = ResolvedBuffer{
        handle,
        binding.buffer,
        binding.size,
        binding.usage,
        binding.state,
        binding.allocationGeneration,
        binding.contentsValid,
        true,
    };
    }
    if (handle.index < m_executionResourceStates.size()) {
        const auto& shadow = m_executionResourceStates[handle.index];
        if (shadow.resource == resourceHandle(handle)) {
            resolved.state = shadow.state;
            resolved.contentsValid = shadow.contentsValid;
        }
    }
    return resolved;
}

void RenderPipeline::setResolvedImageState(
    ImageHandle handle,
    ResourceState2 state,
    bool contentsValid)
{
    const ResourceDesc& logical = m_renderGraph.resource(handle);
    if (!logical.external()) {
        m_resourcePool.setImageState(handle, state, contentsValid);
        return;
    }
    ExternalImageBinding& binding = m_externalImageBindings.at(handle.index);
    binding.state = state;
    binding.contentsValid = contentsValid;
}

void RenderPipeline::setResolvedBufferState(
    BufferHandle handle,
    ResourceState2 state,
    bool contentsValid)
{
    const ResourceDesc& logical = m_renderGraph.resource(handle);
    if (!logical.external()) {
        m_resourcePool.setBufferState(handle, state, contentsValid);
        return;
    }
    ExternalBufferBinding& binding = m_externalBufferBindings.at(handle.index);
    binding.state = state;
    binding.contentsValid = contentsValid;
}

ExportedImage RenderPipeline::exportImage(ImageHandle handle) const
{
    if (!m_compiled || !m_executeCompleted) {
        throw std::runtime_error(
            "RenderGraph image export requires a completed execute");
    }
    const ResourceDesc& logical = m_renderGraph.resource(handle);
    if (!logical.exported) {
        throw std::invalid_argument(
            "RenderGraph image was not declared for export: " + logical.name);
    }
    ResolvedImage resolved =
        const_cast<RenderPipeline*>(this)->resolveImagePhysical(handle);
    if (!resolved.contentsValid) {
        throw std::runtime_error(
            "RenderGraph image export has invalid contents: " + logical.name);
    }
    return ExportedImage{
        resolved,
        m_exportLifetime->epoch,
        m_exportLifetime->frameSerial,
        m_exportLifetime,
    };
}

ExportedBuffer RenderPipeline::exportBuffer(BufferHandle handle) const
{
    if (!m_compiled || !m_executeCompleted) {
        throw std::runtime_error(
            "RenderGraph buffer export requires a completed execute");
    }
    const ResourceDesc& logical = m_renderGraph.resource(handle);
    if (!logical.exported) {
        throw std::invalid_argument(
            "RenderGraph buffer was not declared for export: " + logical.name);
    }
    ResolvedBuffer resolved =
        const_cast<RenderPipeline*>(this)->resolveBufferPhysical(handle);
    if (!resolved.contentsValid) {
        throw std::runtime_error(
            "RenderGraph buffer export has invalid contents: " + logical.name);
    }
    return ExportedBuffer{
        resolved,
        m_exportLifetime->epoch,
        m_exportLifetime->frameSerial,
        m_exportLifetime,
    };
}

void RenderPipeline::executeOverlay(
    CommandList& cmd,
    const std::function<void(VkCommandBuffer)>& draw)
{
    if (!draw) return;
    const auto handle = m_renderGraph.findImage(runtime_resource::swapChainColor);
    if (!handle) {
        throw std::runtime_error(
            "Runtime overlay requires SwapChainColor in the graph");
    }
    ResolvedImage color = resolveImagePhysical(*handle);
    const ResourceState2 overlayState{
        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT
            | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkImageMemoryBarrier2 barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
    barrier.srcStageMask = color.state.stages;
    barrier.srcAccessMask = color.state.access;
    barrier.dstStageMask = overlayState.stages;
    barrier.dstAccessMask = overlayState.access;
    barrier.oldLayout = color.state.layout;
    barrier.newLayout = overlayState.layout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = color.image;
    barrier.subresourceRange = {color.aspect, 0, 1, 0, 1};
    cmd.pipelineBarrier2(std::span<const VkImageMemoryBarrier2>(&barrier, 1), {});
    color.state = overlayState;
    color.currentLayout = overlayState.layout;
    ++m_executeDebug.resourceTransitions;

    const ExternalImageBinding& colorBinding =
        m_externalImageBindings.at(handle->index);
    VkRenderingAttachmentInfo colorAttachment{};
    colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    colorAttachment.imageView = color.imageView;
    colorAttachment.imageLayout = color.currentLayout;
    colorAttachment.loadOp = color.contentsValid
        ? VK_ATTACHMENT_LOAD_OP_LOAD
        : colorBinding.defaultLoadOp;
    if (colorAttachment.loadOp == VK_ATTACHMENT_LOAD_OP_LOAD
        && !color.contentsValid) {
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    }
    colorAttachment.storeOp = colorBinding.defaultStoreOp;
    colorAttachment.clearValue = color.clearValue;

    VkRenderingInfo renderingInfo{};
    renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderingInfo.renderArea.extent = color.extent;
    renderingInfo.layerCount = 1;
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachments = &colorAttachment;
    renderingInfo.pDepthAttachment = nullptr;
    vkCmdBeginRendering(cmd, &renderingInfo);
    ++m_executeDebug.renderingScopes;
    try {
        draw(cmd.cmd());
    } catch (...) {
        vkCmdEndRendering(cmd);
        throw;
    }
    vkCmdEndRendering(cmd);
    setResolvedImageState(
        *handle,
        overlayState,
        colorAttachment.storeOp == VK_ATTACHMENT_STORE_OP_STORE);
}

void RenderPipeline::finalizeExternalImages(CommandList& cmd)
{
    for (auto& [index, binding] : m_externalImageBindings) {
        (void)index;
        if (binding.finalState.layout == VK_IMAGE_LAYOUT_UNDEFINED
            || binding.state == binding.finalState) {
            continue;
        }
        VkImageMemoryBarrier2 barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
        barrier.srcStageMask = binding.state.stages;
        barrier.srcAccessMask = binding.state.access;
        barrier.dstStageMask = binding.finalState.stages;
        barrier.dstAccessMask = binding.finalState.access;
        barrier.oldLayout = binding.state.layout;
        barrier.newLayout = binding.finalState.layout;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = binding.image;
        barrier.subresourceRange = {binding.aspect, 0, 1, 0, 1};
        cmd.pipelineBarrier2(
            std::span<const VkImageMemoryBarrier2>(&barrier, 1), {});
        binding.state = binding.finalState;
        ++m_executeDebug.resourceTransitions;
    }
    for (auto& [index, binding] : m_externalBufferBindings) {
        (void)index;
        if ((binding.finalState.stages == VK_PIPELINE_STAGE_2_NONE
                && binding.finalState.access == VK_ACCESS_2_NONE)
            || binding.state == binding.finalState) {
            continue;
        }
        VkBufferMemoryBarrier2 barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
        barrier.srcStageMask = binding.state.stages;
        barrier.srcAccessMask = binding.state.access;
        barrier.dstStageMask = binding.finalState.stages;
        barrier.dstAccessMask = binding.finalState.access;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.buffer = binding.buffer;
        barrier.offset = 0;
        barrier.size = binding.size;
        cmd.pipelineBarrier2(
            {}, std::span<const VkBufferMemoryBarrier2>(&barrier, 1));
        binding.state = binding.finalState;
        ++m_executeDebug.resourceTransitions;
    }
}

bool RenderPipeline::externalContentsValid(std::string_view resourceName) const
{
    const auto handle = m_renderGraph.findImage(resourceName);
    if (!handle) return false;
    const auto found = m_externalImageBindings.find(handle->index);
    return found != m_externalImageBindings.end() && found->second.contentsValid;
}

std::optional<CommandListStatistics>
RenderPipeline::expectedFrameStatistics() const
{
    CommandStatisticsAccumulator total;
    for (const auto& pass : m_passes) {
        if (!pass->enabled()) continue;
        const auto expected = pass->expectedFrameStatistics();
        if (!expected) return std::nullopt;
        total.add(*expected);
    }
    return total.statistics();
}

void RenderPipeline::clearExternalResources()
{
    m_externalImageBindings.clear();
    m_externalBufferBindings.clear();
    invalidateExports();
}

void RenderPipeline::invalidateExports() noexcept
{
    if (m_exportLifetime) ++m_exportLifetime->epoch;
    m_executeCompleted = false;
}

void RenderPipeline::onResize(uint32_t width, uint32_t height)
{
    if (!m_compiled || !m_device) {
        throw std::runtime_error(
            "RenderPipeline resize requires a compiled graph and live device");
    }
    m_compiled = false;
    invalidateExports();
    try {
        RHIRenderGraphResourceAllocator allocator(*m_device);
        RenderGraphResourcePool candidate =
            RenderGraphResourcePool::buildResizeCandidate(
                m_renderGraph,
                {width, height},
                m_nextAllocationGeneration,
                allocator);
        for (auto& pass : m_passes) pass->onResize(width, height);
        candidate.adoptAbsoluteAllocationsFrom(m_resourcePool);
        m_resourcePool.swap(candidate);
        m_externalImageBindings.clear();
        m_externalBufferBindings.clear();
        ++m_nextAllocationGeneration;
        m_compiled = true;
    } catch (...) {
        m_compiled = false;
        throw;
    }
}

void RenderPipeline::drawRenderGraphUIContent()
{
    ImGui::Text(
        "Compile: passes=%d resources=%d deps=%d barriers=%d",
        static_cast<int>(m_compileDebug.passCount),
        static_cast<int>(m_compileDebug.resourceCount),
        static_cast<int>(m_compileDebug.dependencyCount),
        static_cast<int>(m_compileDebug.barrierCount));
    ImGui::Text(
        "Order: %s",
        m_compileDebug.orderSummary.empty()
            ? "none"
            : m_compileDebug.orderSummary.c_str());

    if (ImGui::CollapsingHeader("Dependencies", ImGuiTreeNodeFlags_DefaultOpen)) {
        for (const PassDependencyEdge& dependency : m_renderGraph.dependencies()) {
            ImGui::BulletText(
                "%s -> %s (%s)",
                m_renderGraph.passes()[dependency.fromPass].name.c_str(),
                m_renderGraph.passes()[dependency.toPass].name.c_str(),
                dependency.explicitDependency ? "explicit" : "resource");
        }
    }
    if (ImGui::CollapsingHeader("Barrier Plan", ImGuiTreeNodeFlags_DefaultOpen)) {
        for (const BarrierPlanItem& barrier : m_renderGraph.barrierPlan()) {
            ImGui::BulletText(
                "%s -> %s | %s (%s)",
                m_renderGraph.passes()[barrier.fromPass].name.c_str(),
                m_renderGraph.passes()[barrier.toPass].name.c_str(),
                m_renderGraph.resources()[barrier.resource.index].name.c_str(),
                toString(barrier.hazard));
        }
    }
    if (ImGui::CollapsingHeader("Last Execute", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Text(
            "Frame=%d planned=%d applied=%d transitions=%d scopes=%d",
            static_cast<int>(m_executeDebug.frameIndex),
            static_cast<int>(m_executeDebug.plannedBarriers),
            static_cast<int>(m_executeDebug.appliedBarriers),
            static_cast<int>(m_executeDebug.resourceTransitions),
            static_cast<int>(m_executeDebug.renderingScopes));
        for (const BarrierDebugEvent& event : m_barrierDebugEvents) {
            if (event.applied) {
                ImGui::BulletText(
                    "%s -> %s | %s (%s) | %s -> %s",
                    event.fromPass.c_str(), event.toPass.c_str(),
                    event.resourceName.c_str(), toString(event.hazard),
                    toString(event.oldLayout), toString(event.newLayout));
            } else {
                ImGui::BulletText(
                    "%s -> %s | %s (%s) | skipped: %s",
                    event.fromPass.c_str(), event.toPass.c_str(),
                    event.resourceName.c_str(), toString(event.hazard),
                    event.reason.c_str());
            }
        }
    }
}

void RenderPipeline::drawPassUIContent()
{
    for (size_t passIndex = 0; passIndex < m_passes.size(); ++passIndex) {
        auto& pass = m_passes[passIndex];
        if (!pass->enabled()) continue;
        const std::string passName(pass->name());
        ImGui::PushID(static_cast<int>(passIndex));
        ImGui::SeparatorText(passName.c_str());
        pass->drawUI();
        ImGui::PopID();
    }
}

} // namespace ku
