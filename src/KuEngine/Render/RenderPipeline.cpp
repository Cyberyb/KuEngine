#include "RenderPipeline.h"
#include "RenderPass.h"
#include "RenderGraph.h"
#include "../RHI/RHIDevice.h"
#include "../RHI/CommandList.h"
#include "../Core/Log.h"

#include <imgui.h>

#include <algorithm>
#include <sstream>
#include <cstdint>
#include <stdexcept>
#include <unordered_set>

namespace ku {

namespace {

const char* toString(ResourceAccessType access)
{
    switch (access) {
        case ResourceAccessType::Read:
            return "R";
        case ResourceAccessType::Write:
            return "W";
        default:
            return "?";
    }
}

const char* toString(ResourceHazardType hazard)
{
    switch (hazard) {
        case ResourceHazardType::ReadAfterWrite:
            return "RAW";
        case ResourceHazardType::WriteAfterRead:
            return "WAR";
        case ResourceHazardType::WriteAfterWrite:
            return "WAW";
        default:
            return "?";
    }
}

const char* toString(VkImageLayout layout)
{
    switch (layout) {
        case VK_IMAGE_LAYOUT_UNDEFINED:
            return "UNDEFINED";
        case VK_IMAGE_LAYOUT_GENERAL:
            return "GENERAL";
        case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
            return "COLOR_ATTACHMENT_OPTIMAL";
        case VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL:
            return "DEPTH_ATTACHMENT_OPTIMAL";
        case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
            return "SHADER_READ_ONLY_OPTIMAL";
        case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
            return "TRANSFER_SRC_OPTIMAL";
        case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
            return "TRANSFER_DST_OPTIMAL";
        case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
            return "PRESENT_SRC_KHR";
        default:
            return "OTHER";
    }
}

enum class PassAccessMode : uint8_t {
    None,
    Read,
    Write,
    ReadWrite,
};

PassAccessMode getPassAccessMode(const PassNode& node, uint32_t resourceId)
{
    bool read = false;
    bool write = false;
    for (const PassResourceAccess& access : node.accesses) {
        if (access.resource.id != resourceId) {
            continue;
        }

        if (access.access == ResourceAccessType::Read) {
            read = true;
        }
        if (access.access == ResourceAccessType::Write) {
            write = true;
        }
    }

    if (read && write) {
        return PassAccessMode::ReadWrite;
    }
    if (write) {
        return PassAccessMode::Write;
    }
    if (read) {
        return PassAccessMode::Read;
    }

    return PassAccessMode::None;
}

VkPipelineStageFlags stageForLayout(VkImageLayout layout)
{
    switch (layout) {
        case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
            return VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        case VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL:
        case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
            return VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT
                | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
            return VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
            return VK_PIPELINE_STAGE_TRANSFER_BIT;
        case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
            return VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
        case VK_IMAGE_LAYOUT_GENERAL:
            return VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
        case VK_IMAGE_LAYOUT_UNDEFINED:
        default:
            return VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    }
}

const PassAttachment* findAttachment(
    const PassNode& node,
    uint32_t resourceId)
{
    const auto found = std::find_if(
        node.attachments.begin(),
        node.attachments.end(),
        [resourceId](const PassAttachment& attachment) {
            return attachment.resource.id == resourceId;
        });
    return found == node.attachments.end() ? nullptr : &*found;
}

VkImageLayout targetLayoutForAccess(
    const PassNode& node,
    uint32_t resourceId,
    VkImageAspectFlags aspect)
{
    if (const PassAttachment* attachment =
            findAttachment(node, resourceId)) {
        return attachment->type == AttachmentType::Depth
            ? VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL
            : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    }

    const PassAccessMode mode = getPassAccessMode(node, resourceId);
    switch (mode) {
        case PassAccessMode::Write:
            if ((aspect & (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT)) != 0) {
                return VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
            }
            return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        case PassAccessMode::Read:
            return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        case PassAccessMode::ReadWrite:
            return VK_IMAGE_LAYOUT_GENERAL;
        case PassAccessMode::None:
        default:
            return VK_IMAGE_LAYOUT_GENERAL;
    }
}

VkPipelineStageFlags targetStageForAccess(
    const PassNode& node,
    uint32_t resourceId,
    VkImageAspectFlags aspect)
{
    if (const PassAttachment* attachment =
            findAttachment(node, resourceId)) {
        return attachment->type == AttachmentType::Depth
            ? VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT
                | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT
            : VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    }

    const PassAccessMode mode = getPassAccessMode(node, resourceId);
    switch (mode) {
        case PassAccessMode::Write:
            if ((aspect & (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT)) != 0) {
                return VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT
                    | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
            }
            return VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        case PassAccessMode::Read:
            return VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        case PassAccessMode::ReadWrite:
            return VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
        case PassAccessMode::None:
        default:
            return VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    }
}

VkAttachmentLoadOp resolveLoadOp(
    AttachmentLoadPolicy policy,
    VkAttachmentLoadOp runtimeDefault)
{
    switch (policy) {
        case AttachmentLoadPolicy::RuntimeDefault:
            return runtimeDefault;
        case AttachmentLoadPolicy::Load:
            return VK_ATTACHMENT_LOAD_OP_LOAD;
        case AttachmentLoadPolicy::Clear:
            return VK_ATTACHMENT_LOAD_OP_CLEAR;
        case AttachmentLoadPolicy::DontCare:
            return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        default:
            return runtimeDefault;
    }
}

VkAttachmentStoreOp resolveStoreOp(
    AttachmentStorePolicy policy,
    VkAttachmentStoreOp runtimeDefault)
{
    switch (policy) {
        case AttachmentStorePolicy::RuntimeDefault:
            return runtimeDefault;
        case AttachmentStorePolicy::Store:
            return VK_ATTACHMENT_STORE_OP_STORE;
        case AttachmentStorePolicy::DontCare:
            return VK_ATTACHMENT_STORE_OP_DONT_CARE;
        default:
            return runtimeDefault;
    }
}

} // namespace

RenderPipeline::~RenderPipeline()
{
    KU_INFO("RenderPipeline destroyed ({} passes)", m_passes.size());
}

void RenderPipeline::compile(const RenderContext& context)
{
    m_compiled = false;
    m_renderGraph.reset();
    m_compiledExecutionOrder.clear();
    m_externalImageBindings.clear();

    for (auto& pass : m_passes) {
        pass->initialize(context);

        const size_t graphIndex = m_renderGraph.registerPass(*pass);
        auto builder = m_renderGraph.buildPass(graphIndex);
        pass->setup(builder);
    }

    m_renderGraph.compile();

    const auto& graphPasses = m_renderGraph.passes();
    const auto& resources = m_renderGraph.resources();

    const auto& executionOrder = m_renderGraph.executionOrder();
    for (const size_t passIndex : executionOrder) {
        if (passIndex >= graphPasses.size()) {
            continue;
        }

        const auto& node = graphPasses[passIndex];
        if (node.pass != nullptr) {
            m_compiledExecutionOrder.push_back(passIndex);
        }

        std::ostringstream accessSummary;
        for (size_t i = 0; i < node.accesses.size(); ++i) {
            const auto& access = node.accesses[i];
            if (i > 0) {
                accessSummary << ", ";
            }

            if (access.resource.id < resources.size()) {
                accessSummary << toString(access.access) << ":" << resources[access.resource.id].name;
            } else {
                accessSummary << toString(access.access) << ":<invalid-resource>";
            }
        }

        const std::string accessText = accessSummary.str();
        KU_INFO(
            "RenderPass compiled: {} (declared resources: {})",
            node.name,
            accessText.empty() ? "none" : accessText);
    }

    std::ostringstream orderSummary;
    for (size_t i = 0; i < executionOrder.size(); ++i) {
        const size_t passIndex = executionOrder[i];
        if (passIndex >= graphPasses.size()) {
            continue;
        }

        if (i > 0) {
            orderSummary << " -> ";
        }
        orderSummary << graphPasses[passIndex].name;
    }

    for (const PassDependencyEdge& dependency : m_renderGraph.dependencies()) {
        if (dependency.fromPass >= graphPasses.size() || dependency.toPass >= graphPasses.size()) {
            continue;
        }

        KU_INFO(
            "RenderGraph dependency: {} -> {} ({})",
            graphPasses[dependency.fromPass].name,
            graphPasses[dependency.toPass].name,
            dependency.explicitDependency ? "explicit" : "resource");
    }

    for (const BarrierPlanItem& barrier : m_renderGraph.barrierPlan()) {
        if (barrier.fromPass >= graphPasses.size() || barrier.toPass >= graphPasses.size()) {
            continue;
        }

        const char* resourceName = "<invalid-resource>";
        if (barrier.resource.id < resources.size()) {
            resourceName = resources[barrier.resource.id].name.c_str();
        }

        KU_INFO(
            "RenderGraph barrier-plan: {} -> {} | {} ({})",
            graphPasses[barrier.fromPass].name,
            graphPasses[barrier.toPass].name,
            resourceName,
            toString(barrier.hazard));
    }

    KU_INFO(
        "RenderGraph executable compile finished (passes={}, resources={}, dependencies={}, barriers={}, order={})",
        m_renderGraph.passes().size(),
        m_renderGraph.resources().size(),
        m_renderGraph.dependencies().size(),
        m_renderGraph.barrierPlan().size(),
        orderSummary.str().empty() ? "none" : orderSummary.str());

    m_compileDebug.passCount = m_renderGraph.passes().size();
    m_compileDebug.resourceCount = m_renderGraph.resources().size();
    m_compileDebug.dependencyCount = m_renderGraph.dependencies().size();
    m_compileDebug.barrierCount = m_renderGraph.barrierPlan().size();
    m_compileDebug.orderSummary = orderSummary.str();
    m_compiled = true;
}

void RenderPipeline::execute(CommandList& cmd, const FrameData& frame)
{
    if (!m_compiled) {
        throw std::runtime_error(
            "RenderPipeline must be compiled before graph execution");
    }

    m_executeDebug.frameIndex = frame.frameIndex;
    m_executeDebug.plannedBarriers = 0;
    m_executeDebug.appliedBarriers = 0;
    m_executeDebug.resourceTransitions = 0;
    m_executeDebug.renderingScopes = 0;
    m_executeDebug.skippedUnbound = 0;
    m_executeDebug.skippedNoAccess = 0;
    m_barrierDebugEvents.clear();

    if (!m_compiledExecutionOrder.empty()) {
        const auto& graphPasses = m_renderGraph.passes();
        const auto& resources = m_renderGraph.resources();

        for (const size_t passIndex : m_compiledExecutionOrder) {
            if (passIndex >= graphPasses.size()) {
                continue;
            }

            const auto& node = graphPasses[passIndex];
            if (node.pass == nullptr || !node.pass->enabled()) {
                continue;
            }

            for (const BarrierPlanItem& barrier : m_renderGraph.barrierPlan()) {
                if (barrier.toPass != passIndex) {
                    continue;
                }

                ++m_executeDebug.plannedBarriers;

                BarrierDebugEvent debugEvent{};
                debugEvent.hazard = barrier.hazard;
                if (barrier.fromPass < graphPasses.size()) {
                    debugEvent.fromPass = graphPasses[barrier.fromPass].name;
                } else {
                    debugEvent.fromPass = "<invalid-pass>";
                }
                if (barrier.toPass < graphPasses.size()) {
                    debugEvent.toPass = graphPasses[barrier.toPass].name;
                } else {
                    debugEvent.toPass = "<invalid-pass>";
                }
                if (barrier.resource.id < resources.size()) {
                    debugEvent.resourceName = resources[barrier.resource.id].name;
                } else {
                    debugEvent.resourceName = "<invalid-resource>";
                    ++m_executeDebug.skippedNoAccess;
                    debugEvent.reason = "barrier references an invalid resource";
                    m_barrierDebugEvents.push_back(std::move(debugEvent));
                    continue;
                }

                const std::string& resourceName =
                    resources[barrier.resource.id].name;
                auto bindingIt = m_externalImageBindings.find(resourceName);
                if (bindingIt == m_externalImageBindings.end()) {
                    ++m_executeDebug.skippedUnbound;
                    debugEvent.reason = "no external image bound";
                    m_barrierDebugEvents.push_back(std::move(debugEvent));
                    if (resources[barrier.resource.id].external) {
                        throw std::runtime_error(
                            "RenderGraph external image is not bound: "
                            + resourceName);
                    }
                    continue;
                }

                ExternalImageBinding& binding = bindingIt->second;
                if (binding.image == VK_NULL_HANDLE) {
                    ++m_executeDebug.skippedUnbound;
                    debugEvent.reason = "external binding has null image";
                    m_barrierDebugEvents.push_back(std::move(debugEvent));
                    if (resources[barrier.resource.id].external) {
                        throw std::runtime_error(
                            "RenderGraph external image binding is null: "
                            + resourceName);
                    }
                    continue;
                }

                const PassAccessMode mode = getPassAccessMode(node, barrier.resource.id);
                if (mode == PassAccessMode::None) {
                    ++m_executeDebug.skippedNoAccess;
                    debugEvent.reason = "target pass has no access declaration";
                    m_barrierDebugEvents.push_back(std::move(debugEvent));
                    continue;
                }

                const VkImageLayout targetLayout = targetLayoutForAccess(
                    node,
                    barrier.resource.id,
                    binding.aspect);
                const VkPipelineStageFlags srcStage = stageForLayout(binding.currentLayout);
                const VkPipelineStageFlags dstStage = targetStageForAccess(
                    node,
                    barrier.resource.id,
                    binding.aspect);

                debugEvent.oldLayout = binding.currentLayout;
                debugEvent.newLayout = targetLayout;

                cmd.imageBarrier(
                    binding.image,
                    binding.currentLayout,
                    targetLayout,
                    srcStage,
                    dstStage,
                    binding.aspect);

                binding.currentLayout = targetLayout;
                ++m_executeDebug.appliedBarriers;
                debugEvent.applied = true;
                debugEvent.reason = "applied outside rendering scope";
                m_barrierDebugEvents.push_back(std::move(debugEvent));
            }

            executePassNode(cmd, frame, node, resources);
        }
        return;
    }

    if (!m_passes.empty()) {
        throw std::runtime_error(
            "Compiled RenderGraph has no executable order for registered passes");
    }
}

void RenderPipeline::executePassNode(
    CommandList& cmd,
    const FrameData& frame,
    const PassNode& node,
    const std::vector<ResourceDesc>& resources)
{
    for (const PassResourceAccess& access : node.accesses) {
        if (access.resource.id >= resources.size()) {
            throw std::runtime_error(
                "RenderGraph pass access references an invalid resource");
        }

        const ResourceDesc& resource = resources[access.resource.id];
        auto bindingIt = m_externalImageBindings.find(resource.name);
        if (bindingIt == m_externalImageBindings.end()
            || bindingIt->second.image == VK_NULL_HANDLE) {
            if (resource.external) {
                throw std::runtime_error(
                    "RenderGraph external image is not bound: " + resource.name);
            }
            ++m_executeDebug.skippedUnbound;
            continue;
        }

        ExternalImageBinding& binding = bindingIt->second;
        const VkImageLayout targetLayout = targetLayoutForAccess(
            node,
            access.resource.id,
            binding.aspect);
        if (binding.currentLayout == targetLayout) {
            continue;
        }

        cmd.imageBarrier(
            binding.image,
            binding.currentLayout,
            targetLayout,
            stageForLayout(binding.currentLayout),
            targetStageForAccess(
                node,
                access.resource.id,
                binding.aspect),
            binding.aspect);
        binding.currentLayout = targetLayout;
        ++m_executeDebug.resourceTransitions;
    }

    if (node.attachments.empty()) {
        node.pass->execute(cmd, frame);
        return;
    }

    std::vector<VkRenderingAttachmentInfo> colorAttachments;
    colorAttachments.reserve(node.attachments.size());
    VkRenderingAttachmentInfo depthAttachment{};
    bool hasDepthAttachment = false;
    VkExtent2D renderExtent{0, 0};

    struct StoredAttachment {
        ExternalImageBinding* binding = nullptr;
        VkAttachmentStoreOp storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    };
    std::vector<StoredAttachment> storedAttachments;
    storedAttachments.reserve(node.attachments.size());

    for (const PassAttachment& attachment : node.attachments) {
        if (attachment.resource.id >= resources.size()) {
            throw std::runtime_error(
                "RenderGraph attachment references an invalid resource");
        }

        const ResourceDesc& resource = resources[attachment.resource.id];
        auto bindingIt = m_externalImageBindings.find(resource.name);
        if (bindingIt == m_externalImageBindings.end()) {
            throw std::runtime_error(
                "RenderGraph attachment image is not bound: " + resource.name);
        }

        ExternalImageBinding& binding = bindingIt->second;
        if (binding.image == VK_NULL_HANDLE
            || binding.imageView == VK_NULL_HANDLE
            || binding.extent.width == 0
            || binding.extent.height == 0) {
            throw std::runtime_error(
                "RenderGraph attachment binding is incomplete: " + resource.name);
        }

        const bool depthAspect =
            (binding.aspect
                & (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT))
            != 0;
        if ((attachment.type == AttachmentType::Depth) != depthAspect) {
            throw std::runtime_error(
                "RenderGraph attachment type does not match image aspect: "
                + resource.name);
        }

        if (renderExtent.width == 0) {
            renderExtent = binding.extent;
        } else if (renderExtent.width != binding.extent.width
            || renderExtent.height != binding.extent.height) {
            throw std::runtime_error(
                "RenderGraph pass attachments must have matching extents");
        }

        const VkAttachmentLoadOp loadOp = resolveLoadOp(
            attachment.loadPolicy,
            binding.defaultLoadOp);
        if (loadOp == VK_ATTACHMENT_LOAD_OP_LOAD
            && !binding.contentsValid) {
            throw std::runtime_error(
                "RenderGraph attachment LOAD requested before valid contents exist: "
                + resource.name);
        }
        const VkAttachmentStoreOp storeOp = resolveStoreOp(
            attachment.storePolicy,
            binding.defaultStoreOp);

        VkRenderingAttachmentInfo renderingAttachment{};
        renderingAttachment.sType =
            VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        renderingAttachment.imageView = binding.imageView;
        renderingAttachment.imageLayout = binding.currentLayout;
        renderingAttachment.loadOp = loadOp;
        renderingAttachment.storeOp = storeOp;
        renderingAttachment.clearValue = binding.clearValue;

        if (attachment.type == AttachmentType::Depth) {
            depthAttachment = renderingAttachment;
            hasDepthAttachment = true;
        } else {
            colorAttachments.push_back(renderingAttachment);
        }
        storedAttachments.push_back(StoredAttachment{&binding, storeOp});
    }

    VkRenderingInfo renderingInfo{};
    renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderingInfo.renderArea.offset = {0, 0};
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

    VkViewport viewport{};
    viewport.width = static_cast<float>(renderExtent.width);
    viewport.height = static_cast<float>(renderExtent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(cmd, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.extent = renderExtent;
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    try {
        node.pass->execute(cmd, frame);
    } catch (...) {
        vkCmdEndRendering(cmd);
        throw;
    }
    vkCmdEndRendering(cmd);

    for (const StoredAttachment& stored : storedAttachments) {
        stored.binding->contentsValid =
            stored.storeOp == VK_ATTACHMENT_STORE_OP_STORE;
    }
}

void RenderPipeline::update(const FrameData& frame)
{
    if (!m_compiledExecutionOrder.empty()) {
        const auto& graphPasses = m_renderGraph.passes();
        for (const size_t passIndex : m_compiledExecutionOrder) {
            if (passIndex >= graphPasses.size()) {
                continue;
            }

            RenderPass* pass = graphPasses[passIndex].pass;
            if (pass != nullptr && pass->enabled()) {
                pass->update(frame);
            }
        }
        return;
    }

    for (auto& pass : m_passes) {
        if (pass->enabled()) {
            pass->update(frame);
        }
    }
}

void RenderPipeline::bindExternalImage(
    const ExternalImageBindingInfo& info)
{
    if (info.resourceName.empty()) {
        throw std::invalid_argument(
            "External image binding requires a resource name");
    }
    if (info.image == VK_NULL_HANDLE
        || info.imageView == VK_NULL_HANDLE
        || info.extent.width == 0
        || info.extent.height == 0) {
        throw std::invalid_argument(
            "External image binding requires an image, view, and non-zero extent");
    }

    m_externalImageBindings[std::string(info.resourceName)] =
        ExternalImageBinding{
        info.image,
        info.imageView,
        info.extent,
        info.currentLayout,
        info.aspect,
        info.clearValue,
        info.defaultLoadOp,
        info.defaultStoreOp,
        info.finalLayout,
        info.contentsValid,
    };
}

void RenderPipeline::executeOverlay(
    CommandList& cmd,
    const std::function<void(VkCommandBuffer)>& draw)
{
    if (!draw) {
        return;
    }

    const auto colorIt =
        m_externalImageBindings.find(std::string(runtime_resource::swapChainColor));
    if (colorIt == m_externalImageBindings.end()) {
        throw std::runtime_error(
            "Runtime overlay requires the SwapChainColor external image");
    }

    ExternalImageBinding& color = colorIt->second;
    if (color.currentLayout != VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) {
        cmd.imageBarrier(
            color.image,
            color.currentLayout,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            stageForLayout(color.currentLayout),
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            color.aspect);
        color.currentLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        ++m_executeDebug.resourceTransitions;
    }

    VkRenderingAttachmentInfo colorAttachment{};
    colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    colorAttachment.imageView = color.imageView;
    colorAttachment.imageLayout = color.currentLayout;
    colorAttachment.loadOp =
        color.contentsValid ? VK_ATTACHMENT_LOAD_OP_LOAD : color.defaultLoadOp;
    if (colorAttachment.loadOp == VK_ATTACHMENT_LOAD_OP_LOAD
        && !color.contentsValid) {
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    }
    colorAttachment.storeOp = color.defaultStoreOp;
    colorAttachment.clearValue = color.clearValue;

    VkRenderingAttachmentInfo depthAttachment{};
    ExternalImageBinding* depth = nullptr;
    const auto depthIt =
        m_externalImageBindings.find(std::string(runtime_resource::sceneDepth));
    if (depthIt != m_externalImageBindings.end()) {
        depth = &depthIt->second;
        if (depth->extent.width != color.extent.width
            || depth->extent.height != color.extent.height) {
            throw std::runtime_error(
                "Runtime overlay color and depth extents do not match");
        }
        if (depth->currentLayout != VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL) {
            cmd.imageBarrier(
                depth->image,
                depth->currentLayout,
                VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
                stageForLayout(depth->currentLayout),
                VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT
                    | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                depth->aspect);
            depth->currentLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
            ++m_executeDebug.resourceTransitions;
        }

        depthAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        depthAttachment.imageView = depth->imageView;
        depthAttachment.imageLayout = depth->currentLayout;
        depthAttachment.loadOp = depth->contentsValid
            ? VK_ATTACHMENT_LOAD_OP_LOAD
            : VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        depthAttachment.storeOp = depth->contentsValid
            ? depth->defaultStoreOp
            : VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachment.clearValue = depth->clearValue;
    }

    VkRenderingInfo renderingInfo{};
    renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderingInfo.renderArea.offset = {0, 0};
    renderingInfo.renderArea.extent = color.extent;
    renderingInfo.layerCount = 1;
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachments = &colorAttachment;
    renderingInfo.pDepthAttachment = depth ? &depthAttachment : nullptr;

    vkCmdBeginRendering(cmd, &renderingInfo);
    ++m_executeDebug.renderingScopes;
    try {
        draw(cmd.cmd());
    } catch (...) {
        vkCmdEndRendering(cmd);
        throw;
    }
    vkCmdEndRendering(cmd);

    color.contentsValid =
        colorAttachment.storeOp == VK_ATTACHMENT_STORE_OP_STORE;
    if (depth != nullptr) {
        depth->contentsValid =
            depth->contentsValid
            && depthAttachment.storeOp == VK_ATTACHMENT_STORE_OP_STORE;
    }
}

void RenderPipeline::finalizeExternalImages(CommandList& cmd)
{
    for (auto& [resourceName, binding] : m_externalImageBindings) {
        (void)resourceName;
        if (binding.image == VK_NULL_HANDLE
            || binding.finalLayout == VK_IMAGE_LAYOUT_UNDEFINED
            || binding.finalLayout == VK_IMAGE_LAYOUT_GENERAL
            || binding.currentLayout == binding.finalLayout) {
            continue;
        }

        cmd.imageBarrier(
            binding.image,
            binding.currentLayout,
            binding.finalLayout,
            stageForLayout(binding.currentLayout),
            stageForLayout(binding.finalLayout),
            binding.aspect);
        binding.currentLayout = binding.finalLayout;
        ++m_executeDebug.resourceTransitions;
    }
}

bool RenderPipeline::externalContentsValid(
    std::string_view resourceName) const
{
    const auto found =
        m_externalImageBindings.find(std::string(resourceName));
    return found != m_externalImageBindings.end()
        && found->second.contentsValid;
}

void RenderPipeline::clearExternalResources()
{
    m_externalImageBindings.clear();
}

void RenderPipeline::drawUI()
{
    ImGui::SetNextWindowBgAlpha(0.85f);
    ImGui::SetNextWindowSize(ImVec2(460.0f, 0.0f), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("RenderGraph Debug")) {
        drawUIInline();
    }
    ImGui::End();
}

void RenderPipeline::drawUIInline()
{
    ImGui::Text(
        "Compile: passes=%d resources=%d deps=%d barriers=%d",
        static_cast<int>(m_compileDebug.passCount),
        static_cast<int>(m_compileDebug.resourceCount),
        static_cast<int>(m_compileDebug.dependencyCount),
        static_cast<int>(m_compileDebug.barrierCount));

    ImGui::Text(
        "Order: %s",
        m_compileDebug.orderSummary.empty() ? "none" : m_compileDebug.orderSummary.c_str());

    if (ImGui::CollapsingHeader("Dependencies", ImGuiTreeNodeFlags_DefaultOpen)) {
        const auto& passes = m_renderGraph.passes();
        for (const PassDependencyEdge& dependency : m_renderGraph.dependencies()) {
            if (dependency.fromPass >= passes.size() || dependency.toPass >= passes.size()) {
                continue;
            }

            ImGui::BulletText(
                "%s -> %s (%s)",
                passes[dependency.fromPass].name.c_str(),
                passes[dependency.toPass].name.c_str(),
                dependency.explicitDependency ? "explicit" : "resource");
        }
    }

    if (ImGui::CollapsingHeader("Barrier Plan", ImGuiTreeNodeFlags_DefaultOpen)) {
        const auto& passes = m_renderGraph.passes();
        const auto& resources = m_renderGraph.resources();
        for (const BarrierPlanItem& barrier : m_renderGraph.barrierPlan()) {
            if (barrier.fromPass >= passes.size() || barrier.toPass >= passes.size()) {
                continue;
            }

            const char* resourceName = "<invalid-resource>";
            if (barrier.resource.id < resources.size()) {
                resourceName = resources[barrier.resource.id].name.c_str();
            }

            ImGui::BulletText(
                "%s -> %s | %s (%s)",
                passes[barrier.fromPass].name.c_str(),
                passes[barrier.toPass].name.c_str(),
                resourceName,
                toString(barrier.hazard));
        }
    }

    if (ImGui::CollapsingHeader("Last Execute", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Text(
            "Frame=%d planned=%d applied=%d transitions=%d scopes=%d skipped-unbound=%d skipped-no-access=%d",
            static_cast<int>(m_executeDebug.frameIndex),
            static_cast<int>(m_executeDebug.plannedBarriers),
            static_cast<int>(m_executeDebug.appliedBarriers),
            static_cast<int>(m_executeDebug.resourceTransitions),
            static_cast<int>(m_executeDebug.renderingScopes),
            static_cast<int>(m_executeDebug.skippedUnbound),
            static_cast<int>(m_executeDebug.skippedNoAccess));

        for (const BarrierDebugEvent& event : m_barrierDebugEvents) {
            if (event.applied) {
                ImGui::BulletText(
                    "%s -> %s | %s (%s) | %s -> %s",
                    event.fromPass.c_str(),
                    event.toPass.c_str(),
                    event.resourceName.c_str(),
                    toString(event.hazard),
                    toString(event.oldLayout),
                    toString(event.newLayout));
            } else {
                ImGui::BulletText(
                    "%s -> %s | %s (%s) | skipped: %s",
                    event.fromPass.c_str(),
                    event.toPass.c_str(),
                    event.resourceName.c_str(),
                    toString(event.hazard),
                    event.reason.c_str());
            }
        }
    }

    for (auto& pass : m_passes) {
        if (!pass->enabled()) {
            continue;
        }

        if (pass->supportsInlineUI()) {
            if (ImGui::CollapsingHeader(pass->name().data(), ImGuiTreeNodeFlags_DefaultOpen)) {
                pass->drawUIInline();
            }
        } else {
            pass->drawUI();
        }
    }
}

void RenderPipeline::onResize(uint32_t width, uint32_t height)
{
    for (auto& pass : m_passes) {
        pass->onResize(width, height);
    }
}

} // namespace ku
