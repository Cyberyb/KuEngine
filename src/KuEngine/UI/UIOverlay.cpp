#include "UIOverlay.h"
#include "../RHI/RHIDevice.h"
#include "../Core/Log.h"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>

#include <algorithm>
#include <array>
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

void UIOverlay::init(::GLFWwindow* window, VkInstance instance, VkFormat imageFormat, uint32_t imageCount)
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

void UIOverlay::drawFPSPanel(float fps, float deltaTime)
{
    if (!m_initialized) {
        return;
    }

    ImGui::SetNextWindowPos(ImVec2(10.0f, 10.0f), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.35f);
    const ImGuiWindowFlags overlayFlags =
        ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_AlwaysAutoResize |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoFocusOnAppearing |
        ImGuiWindowFlags_NoNav;

    ImGui::Begin("KuEngine Stats", nullptr, overlayFlags);
    ImGui::Text("FPS: %.1f", fps);
    ImGui::Text("Frame: %.2f ms", deltaTime * 1000.0f);
    ImGui::End();
}

void UIOverlay::onSwapChainRecreated(uint32_t imageCount)
{
    if (!m_initialized) {
        return;
    }

    ImGui_ImplVulkan_SetMinImageCount(std::max(2u, imageCount));
}

} // namespace ku
