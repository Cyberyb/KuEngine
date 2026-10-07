#include "UIOverlay.h"
#include "../RHI/RHIDevice.h"
#include "../Core/Log.h"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>

#include <algorithm>
#include <array>
#include <cinttypes>
#include <stdexcept>

namespace ku {

UIOverlay::UIOverlay(
    const RHIDevice& device,
    ::GLFWwindow* window,
    VkInstance instance,
    VkFormat imageFormat,
    uint32_t imageCount)
    : m_device(&device)
{
    init(window, instance, imageFormat, imageCount);
}

UIOverlay::~UIOverlay()
{
    if (!m_initialized) {
        return;
    }

    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    if (m_descriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(m_device->device(), m_descriptorPool, nullptr);
        m_descriptorPool = VK_NULL_HANDLE;
    }

    m_initialized = false;
}

VkDescriptorPool UIOverlay::createDescriptorPool() const
{
    const std::array<VkDescriptorPoolSize, 1> poolSizes = {{
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1024},
    }};

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    poolInfo.maxSets = 1024;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();

    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
    VK_CHECK(vkCreateDescriptorPool(m_device->device(), &poolInfo, nullptr, &descriptorPool));
    return descriptorPool;
}

void UIOverlay::checkVkResult(VkResult result)
{
    if (result == VK_SUCCESS) {
        return;
    }

    KU_ERROR("ImGui Vulkan backend error: {}", static_cast<int>(result));
}

void UIOverlay::init(
    ::GLFWwindow* window,
    VkInstance instance,
    VkFormat imageFormat,
    uint32_t imageCount)
{
    if (m_initialized) {
        return;
    }

    m_descriptorPool = createDescriptorPool();

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui::GetIO().IniFilename = nullptr;

    if (!ImGui_ImplGlfw_InitForVulkan(window, true)) {
        throw std::runtime_error("failed to initialize ImGui GLFW backend");
    }

    VkPipelineRenderingCreateInfo pipelineRenderingInfo{};
    pipelineRenderingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    pipelineRenderingInfo.colorAttachmentCount = 1;
    pipelineRenderingInfo.pColorAttachmentFormats = &imageFormat;
    pipelineRenderingInfo.depthAttachmentFormat = uiOverlayDepthFormat;
    pipelineRenderingInfo.stencilAttachmentFormat = VK_FORMAT_UNDEFINED;

    ImGui_ImplVulkan_InitInfo initInfo{};
    initInfo.ApiVersion = VK_API_VERSION_1_3;
    initInfo.Instance = instance;
    initInfo.PhysicalDevice = m_device->physicalDevice();
    initInfo.Device = m_device->device();
    initInfo.QueueFamily = m_device->graphicsQueueFamily();
    initInfo.Queue = m_device->graphicsQueue();
    initInfo.DescriptorPool = m_descriptorPool;
    initInfo.MinImageCount = 2;
    initInfo.ImageCount = std::max(2u, imageCount);
    initInfo.UseDynamicRendering = true;
    initInfo.PipelineInfoMain.PipelineRenderingCreateInfo = pipelineRenderingInfo;
    initInfo.CheckVkResultFn = checkVkResult;

    if (!ImGui_ImplVulkan_Init(&initInfo)) {
        throw std::runtime_error("failed to initialize ImGui Vulkan backend");
    }

    m_initialized = true;
    KU_INFO("ImGui overlay initialized");
}

void UIOverlay::newFrame()
{
    if (!m_initialized) {
        return;
    }

    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void UIOverlay::render(VkCommandBuffer cmd, VkImageView imageView, VkImageLayout imageLayout)
{
    if (!m_initialized) {
        return;
    }

    (void)imageView;
    (void)imageLayout;

    ImGui::Render();
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
}

SidebarFrameLayout UIOverlay::describeSidebarFrame(
    float logicalWindowWidth,
    float logicalWindowHeight,
    bool showStats) const noexcept
{
    return ku::describeSidebarFrame(
        m_sidebarState,
        m_sidebarLayoutPolicy,
        logicalWindowWidth,
        logicalWindowHeight,
        showStats);
}

void UIOverlay::drawSidebar(
    const SidebarFrameLayout& sidebarFrame,
    const UIFrameStatistics& stats,
    const SidebarContent& parameterContent,
    const SidebarContent& renderGraphContent)
{
    if (!m_initialized) {
        return;
    }

    const SidebarLayoutMode mode = sidebarFrame.mode;
    const SidebarLayout& layout = sidebarFrame.window;
    const SidebarSections& sections = sidebarFrame.sections;

    ImGui::SetNextWindowPos(ImVec2(layout.x, layout.y), ImGuiCond_Always);
    ImGui::SetNextWindowSize(
        ImVec2(layout.width, layout.height),
        ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(mode == SidebarLayoutMode::Expanded ? 0.94f : 0.82f);

    ImGuiWindowFlags sidebarFlags =
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_AlwaysVerticalScrollbar |
        ImGuiWindowFlags_HorizontalScrollbar;

    const char* windowTitle = mode == SidebarLayoutMode::Expanded
        ? "KuEngine Sidebar"
        : "KuEngine Sidebar##Compact";
    ImGui::Begin(windowTitle, nullptr, sidebarFlags);

    if (mode == SidebarLayoutMode::Expanded) {
        if (ImGui::Button("Collapse Sidebar")) {
            m_sidebarState.collapse();
        }

        bool compactStatsVisible = m_sidebarState.compactStatsVisible();
        if (ImGui::Checkbox(
                "Compact stats when collapsed",
                &compactStatsVisible)) {
            m_sidebarState.setCompactStatsVisible(compactStatsVisible);
        }

        if (ImGui::CollapsingHeader(
                "Performance",
                ImGuiTreeNodeFlags_DefaultOpen)) {
            if (sections.performance) {
                drawStatisticsContent(stats, false);
            } else {
                ImGui::TextDisabled("Statistics hidden by runtime settings");
            }
        }

        if (ImGui::CollapsingHeader(
                "Parameters / Scene",
                ImGuiTreeNodeFlags_DefaultOpen)) {
            if (parameterContent) {
                parameterContent();
            } else {
                ImGui::TextDisabled("No parameters available");
            }
        }

        if (ImGui::CollapsingHeader(
                "Render Graph",
                ImGuiTreeNodeFlags_DefaultOpen)) {
            if (renderGraphContent) {
                renderGraphContent();
            } else {
                ImGui::TextDisabled("Render Graph information unavailable");
            }
        }
    } else {
        if (sections.reopenControl && ImGui::Button("Open Sidebar")) {
            m_sidebarState.expand();
        }

        if (sections.performance) {
            ImGui::SeparatorText("Performance");
            drawStatisticsContent(stats, true);
            if (ImGui::Button("Hide compact stats")) {
                m_sidebarState.setCompactStatsVisible(false);
            }
        }
    }

    ImGui::End();
}

void UIOverlay::drawStatisticsContent(
    const UIFrameStatistics& stats,
    bool compact) const
{
    ImGui::Text("Loop FPS: %.1f", stats.fps);
    ImGui::Text("Loop Frame: %.2f ms", stats.frameTimeMilliseconds);
    if (!compact) {
        ImGui::TextDisabled("FPS/Frame: current main-loop sample");
    }
    ImGui::Separator();

    if (!stats.completedFrame.has_value()) {
        ImGui::TextDisabled("Completed Submit: waiting");
        ImGui::TextDisabled("CPU: N/A (waiting)");
        if (stats.gpuStatusBeforeFirstCompletedFrame
            == GpuTimeStatus::Unsupported) {
            ImGui::TextDisabled("GPU: N/A (timestamp unsupported)");
        } else {
            ImGui::TextDisabled("GPU: N/A (waiting)");
        }
        ImGui::TextDisabled("Draw Calls: N/A (waiting)");
        ImGui::TextDisabled("Vertices: N/A (waiting)");
        return;
    }

    const CompletedFrameStatistics& completed = *stats.completedFrame;
    ImGui::Text("Completed Submit: #%" PRIu64, completed.submittedFrame);
    ImGui::Text("CPU: %.2f ms", completed.cpuTimeMilliseconds);
    switch (completed.gpuTime.status) {
        case GpuTimeStatus::Available:
            ImGui::Text("GPU: %.2f ms", completed.gpuTime.milliseconds);
            break;
        case GpuTimeStatus::Unsupported:
            ImGui::TextDisabled("GPU: N/A (timestamp unsupported)");
            break;
        case GpuTimeStatus::Waiting:
        default:
            ImGui::TextDisabled("GPU: N/A (waiting)");
            break;
    }
    ImGui::Text("Draw Calls: %" PRIu64, completed.commands.drawCalls);
    ImGui::Text(
        "Vertices: %" PRIu64,
        completed.commands.submittedVertices);
    if (!compact) {
        ImGui::TextDisabled("CPU/GPU/Draw/Vertices: same completed submit");
    }
}

void UIOverlay::onSwapChainRecreated(uint32_t imageCount)
{
    if (!m_initialized) {
        return;
    }

    ImGui_ImplVulkan_SetMinImageCount(std::max(2u, imageCount));
}

} // namespace ku
