#include "TrianglePass.h"
#include <KuEngine/RHI/CommandList.h>
#include <KuEngine/Render/RenderGraph.h>

#include <KuEngine/Core/Log.h>
#include <imgui.h>
#include <filesystem>

namespace ku {

TrianglePass::TrianglePass() = default;
TrianglePass::~TrianglePass() = default;

void TrianglePass::setup(RenderGraphBuilder& builder)
{
    const ResourceHandle swapChainColor = builder.importExternal("SwapChainColor");
    builder.write(swapChainColor);
}

void TrianglePass::initialize(RHIDevice& device)
{
    KU_INFO("TrianglePass: initializing...");

    std::filesystem::path shaderDir = std::filesystem::current_path() / "shaders";
    auto vertPath = shaderDir / "triangle.vert.spv";
    auto fragPath = shaderDir / "triangle.frag.spv";

    KU_DEBUG("Loading shaders from: {}", shaderDir.string());

    try {
        m_vertShader = std::make_unique<RHIShader>(device, vertPath);
        m_fragShader = std::make_unique<RHIShader>(device, fragPath);
    } catch (const std::exception& e) {
        KU_ERROR("Failed to load shaders: {}", e.what());
        KU_ERROR("  Expected at: {}", vertPath.string());
        KU_ERROR("  Expected at: {}", fragPath.string());
        return;
    }

    GraphicsPipelineDesc desc{};
    desc.shaders.push_back(*m_vertShader);
    desc.shaders.push_back(*m_fragShader);
    desc.colorFormats = {VK_FORMAT_B8G8R8A8_UNORM};
    desc.pushConstantRanges = {
        VkPushConstantRange{VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(float) * 4}
    };
    desc.depthFormat = VK_FORMAT_UNDEFINED;
    desc.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    desc.cullMode = VK_CULL_MODE_NONE;
    desc.depthTest = false;
    desc.depthWrite = false;

    m_pipeline = std::make_unique<RHIPipeline>(device, desc);
    KU_INFO("TrianglePass: initialized");
}

void TrianglePass::execute(CommandList& cmd, const FrameData&)
{
    if (!m_pipeline) return;

    m_pipeline->bind(cmd);
    vkCmdPushConstants(
        cmd,
        m_pipeline->layout(),
        VK_SHADER_STAGE_FRAGMENT_BIT,
        0,
        sizeof(float) * 4,
        m_triangleColor.data());

    vkCmdDraw(cmd, 3, 1, 0, 0);
}

void TrianglePass::drawUI()
{
    ImGui::Begin("Triangle Controls");
    ImGui::ColorEdit4("Color", m_triangleColor.data());
    ImGui::End();
}

void TrianglePass::onResize(uint32_t width, uint32_t height)
{
    KU_DEBUG("TrianglePass: resize {}x{}", width, height);
}

} // namespace ku
