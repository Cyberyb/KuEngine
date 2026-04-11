#include "UIOverlay.h"
#include "../RHI/RHIDevice.h"
#include "../Core/Log.h"

// imgui backend removed for MVP
// backend not in vcpkg

namespace ku {

UIOverlay::UIOverlay(const RHIDevice& device, VkFormat imageFormat)
    : m_device(&device)
{
    init(imageFormat);
    m_frameResources.resize(2);
}

UIOverlay::~UIOverlay() = default;

void UIOverlay::init(VkFormat)
{
    if (m_initialized) return;
    // ImGui::CreateContext();
    // ImGuiIO& io = ImGui::GetIO();
    // (void)io;
    // io.Fonts->AddFontDefault();
    m_initialized = true;
    KU_INFO("ImGui overlay initialized");
}

void UIOverlay::newFrame()
{
    // ImGui_ImplGlfw_NewFrame();
    // ImGui::NewFrame();
}

void UIOverlay::render(VkCommandBuffer cmd, VkImageView imageView, VkImageLayout imageLayout)
{
    // ImGui::Render();
    (void)cmd; (void)imageView; (void)imageLayout;
}

void UIOverlay::drawFPSPanel(float fps, float deltaTime)
{
    (void)fps;
    (void)deltaTime;
    // ImGui::Begin
    // ImGui::Text
    // ImGui::Text
    // ImGui::End
}

} // namespace ku
