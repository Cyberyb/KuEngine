#include "UIOverlay.h"

#include "RHIDevice.h"

#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_vulkan.h>

namespace ku {

UIOverlay::UIOverlay(const RHIDevice& device, VkFormat imageFormat) : m_device(&device) {
    init(imageFormat);
}

UIOverlay::~UIOverlay() {
    if (m_initialized) {
        for (auto& res : m_frameResources) {
            vkDestroyDescriptorPool(m_device->device(), res.descriptorPool, nullptr);
        }
        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplSDL3_Shutdown();
        ImGui::DestroyContext();
    }
}

void UIOverlay::init(VkFormat imageFormat) {
    // 创建 ImGui context
    IMGUI_CHECKVERSION();
    m_imguiContext = ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    // 设置后端
    ImGui_ImplSDL3_InitForVulkan(m_device->device()); // 简化版
    ImGui_ImplVulkan_InitInfo initInfo{};
    initInfo.Instance = m_device->device(); // 需要传入真实 instance
    initInfo.PhysicalDevice = m_device->physicalDevice();
    initInfo.Device = m_device->device();
    initInfo.QueueFamily = m_device->graphicsQueueFamily();
    initInfo.Queue = m_device->graphicsQueue();
    initInfo.PipelineCache = VK_NULL_HANDLE;
    initInfo.DescriptorPool = VK_NULL_HANDLE;
    initInfo.Subpass = 0;
    initInfo.MinImageCount = 2;
    initInfo.ImageCount = 2;
    initInfo.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    initInfo.Allocator = nullptr;
    initInfo.CheckVkResultFn = nullptr;

    // ImGui_ImplVulkan_Init(&initInfo, renderPass);

    m_initialized = true;
}

void UIOverlay::newFrame() {
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
}

void UIOverlay::drawFPSPanel(float fps, float deltaTime) {
    ImGui::Begin("FPS", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
    ImGui::Text("FPS: %.1f", fps);
    ImGui::Text("Delta: %.3f ms", deltaTime * 1000.0f);
    ImGui::End();
}

void UIOverlay::render(VkCommandBuffer cmd, VkImageView imageView, VkImageLayout imageLayout) {
    ImGui::Render();
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
}

} // namespace ku
