#include "RenderPipeline.h"
#include "RenderPass.h"
#include "RenderGraph.h"
#include "../RHI/RHIDevice.h"
#include "../RHI/CommandList.h"
#include "../Core/Log.h"

#include <imgui.h>

#include <sstream>
#include <cstdint>

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

VkImageLayout targetLayoutForAccess(PassAccessMode mode)
{
    switch (mode) {
        case PassAccessMode::Write:
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

VkPipelineStageFlags targetStageForAccess(PassAccessMode mode)
{
    switch (mode) {
        case PassAccessMode::Write:
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

} // namespace

RenderPipeline::~RenderPipeline()
{
    KU_INFO("RenderPipeline destroyed ({} passes)", m_passes.size());
}

void RenderPipeline::compile(RHIDevice& device)
{
    m_renderGraph.reset();
    m_compiledExecutionOrder.clear();
    m_resourceNameToId.clear();

    for (auto& pass : m_passes) {
        pass->initialize(device);

        const size_t graphIndex = m_renderGraph.registerPass(*pass);
        auto builder = m_renderGraph.buildPass(graphIndex);
        pass->setup(builder);
    }

    m_renderGraph.compile();

    const auto& graphPasses = m_renderGraph.passes();
    const auto& resources = m_renderGraph.resources();

    m_resourceNameToId.reserve(resources.size());
    for (size_t i = 0; i < resources.size(); ++i) {
        m_resourceNameToId[resources[i].name] = static_cast<uint32_t>(i);
    }

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
        "RenderGraph stage-2 compile finished (passes={}, resources={}, dependencies={}, barriers={}, order={})",
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
}

void RenderPipeline::execute(CommandList& cmd, const FrameData& frame)
{
    m_executeDebug.frameIndex = frame.frameIndex;
    m_executeDebug.plannedBarriers = 0;
    m_executeDebug.appliedBarriers = 0;
    m_executeDebug.skippedUnbound = 0;
    m_executeDebug.skippedNoAccess = 0;
    m_executeDebug.skippedNoop = 0;
    m_executeDebug.skippedInRendering = 0;
    m_barrierDebugEvents.clear();

    if (!m_compiledExecutionOrder.empty()) {
        const auto& graphPasses = m_renderGraph.passes();
        const auto& resources = m_renderGraph.resources();

        for (const size_t passIndex : m_compiledExecutionOrder) {
            if (passIndex >= graphPasses.size()) {
                continue;
            }

            const auto& node = graphPasses[passIndex];

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
                }

                auto bindingIt = m_externalImageBindings.find(barrier.resource.id);
                if (bindingIt == m_externalImageBindings.end()) {
                    ++m_executeDebug.skippedUnbound;
                    debugEvent.reason = "no external image bound";
                    m_barrierDebugEvents.push_back(std::move(debugEvent));
                    continue;
                }

                ExternalImageBinding& binding = bindingIt->second;
                if (binding.image == VK_NULL_HANDLE) {
                    ++m_executeDebug.skippedUnbound;
                    debugEvent.reason = "external binding has null image";
                    m_barrierDebugEvents.push_back(std::move(debugEvent));
                    continue;
                }

                const PassAccessMode mode = getPassAccessMode(node, barrier.resource.id);
                if (mode == PassAccessMode::None) {
                    ++m_executeDebug.skippedNoAccess;
                    debugEvent.reason = "target pass has no access declaration";
                    m_barrierDebugEvents.push_back(std::move(debugEvent));
                    continue;
                }

                const VkImageLayout targetLayout = targetLayoutForAccess(mode);
                const VkPipelineStageFlags srcStage = stageForLayout(binding.currentLayout);
                const VkPipelineStageFlags dstStage = targetStageForAccess(mode);

                debugEvent.oldLayout = binding.currentLayout;
                debugEvent.newLayout = targetLayout;

                // Avoid issuing no-op image barriers. In dynamic rendering regions
                // these can trigger VUID-08719 (only memory barriers are allowed).
                if (binding.currentLayout == targetLayout) {
                    ++m_executeDebug.skippedNoop;
                    debugEvent.reason = "layout already matched (no-op barrier skipped)";
                    m_barrierDebugEvents.push_back(std::move(debugEvent));
                    continue;
                }

                if (m_executeInsideRendering) {
                    ++m_executeDebug.skippedInRendering;
                    debugEvent.reason = "layout transition skipped inside rendering scope";
                    m_barrierDebugEvents.push_back(std::move(debugEvent));
                    continue;
                }

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
                debugEvent.reason = "applied";
                m_barrierDebugEvents.push_back(std::move(debugEvent));
            }

            if (node.pass != nullptr && node.pass->enabled()) {
                node.pass->execute(cmd, frame);
            }
        }
        return;
    }

    // Fallback path: allows execution even if compile() was not called.
    for (auto& pass : m_passes) {
        if (pass->enabled()) {
            pass->execute(cmd, frame);
        }
    }
}

void RenderPipeline::bindExternalImage(
    std::string_view resourceName,
    VkImage image,
    VkImageLayout currentLayout,
    VkImageAspectFlags aspect)
{
    const auto resourceIt = m_resourceNameToId.find(std::string(resourceName));
    if (resourceIt == m_resourceNameToId.end()) {
        return;
    }

    m_externalImageBindings[resourceIt->second] = ExternalImageBinding{
        image,
        currentLayout,
        aspect,
    };
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
                "Frame=%d planned=%d applied=%d skipped-unbound=%d skipped-no-access=%d skipped-noop=%d skipped-in-rendering=%d",
                static_cast<int>(m_executeDebug.frameIndex),
                static_cast<int>(m_executeDebug.plannedBarriers),
                static_cast<int>(m_executeDebug.appliedBarriers),
                static_cast<int>(m_executeDebug.skippedUnbound),
                static_cast<int>(m_executeDebug.skippedNoAccess),
                static_cast<int>(m_executeDebug.skippedNoop),
                static_cast<int>(m_executeDebug.skippedInRendering));

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
    }
    ImGui::End();

    for (auto& pass : m_passes) {
        if (pass->enabled()) {
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
