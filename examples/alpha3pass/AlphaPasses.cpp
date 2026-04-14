#include "AlphaPasses.h"

#include <KuEngine/Core/Log.h>
#include <KuEngine/Render/RenderGraph.h>

#include <imgui.h>

#include <cstring>
#include <filesystem>

namespace ku {

AlphaShapePass::AlphaShapePass(
    std::string name,
    std::array<float, 4> color,
    std::array<float, 2> offset,
    std::array<float, 2> scale,
    std::string dependency)
    : m_name(std::move(name))
    , m_dependency(std::move(dependency))
    , m_color(color)
    , m_offset(offset)
    , m_scale(scale)
{
}

AlphaShapePass::~AlphaShapePass() = default;

void AlphaShapePass::initialize(RHIDevice& device)
{
    KU_INFO("{}: initializing...", m_name);

    const std::filesystem::path shaderDir = std::filesystem::current_path() / "shaders";
    const auto vertPath = shaderDir / "alpha.vert.spv";
    const auto fragPath = shaderDir / "alpha.frag.spv";

    try {
        m_vertShader = std::make_unique<RHIShader>(device, vertPath);
        m_fragShader = std::make_unique<RHIShader>(device, fragPath);
    } catch (const std::exception& e) {
        KU_ERROR("{}: shader load failed: {}", m_name, e.what());
        return;
    }

    GraphicsPipelineDesc desc{};
    desc.shaders.push_back(*m_vertShader);
    desc.shaders.push_back(*m_fragShader);
    desc.colorFormats = {VK_FORMAT_B8G8R8A8_UNORM};
    desc.pushConstantRanges = {
        VkPushConstantRange{
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
            0,
            sizeof(PushConstants)}
    };
    desc.depthFormat = VK_FORMAT_UNDEFINED;
    desc.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    desc.cullMode = VK_CULL_MODE_NONE;
    desc.depthTest = false;
    desc.depthWrite = false;

    m_pipeline = std::make_unique<RHIPipeline>(device, desc);
    KU_INFO("{}: initialized", m_name);
}

void AlphaShapePass::setup(RenderGraphBuilder& builder)
{
    const ResourceHandle swapChainColor = builder.importExternal("SwapChainColor");
    builder.write(swapChainColor);

    if (!m_dependency.empty()) {
        builder.dependsOn(m_dependency);
    }
}

void AlphaShapePass::execute(CommandList& cmd, const FrameData&)
{
    if (!m_pipeline) {
        return;
    }

    PushConstants pc{};
    std::memcpy(pc.color, m_color.data(), sizeof(pc.color));
    std::memcpy(pc.offset, m_offset.data(), sizeof(pc.offset));
    std::memcpy(pc.scale, m_scale.data(), sizeof(pc.scale));

    m_pipeline->bind(cmd);
    vkCmdPushConstants(
        cmd,
        m_pipeline->layout(),
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        0,
        sizeof(PushConstants),
        &pc);

    vkCmdDraw(cmd, 3, 1, 0, 0);
}

void AlphaShapePass::drawUI()
{
    const std::string panelName = m_name + " Pass";
    ImGui::Begin(panelName.c_str());
    ImGui::ColorEdit4("Color", m_color.data());
    ImGui::SliderFloat2("Offset", m_offset.data(), -1.0f, 1.0f);
    ImGui::SliderFloat2("Scale", m_scale.data(), 0.1f, 2.0f);
    ImGui::End();
}

void AlphaShapePass::onResize(uint32_t width, uint32_t height)
{
    KU_DEBUG("{}: resize {}x{}", m_name, width, height);
}

} // namespace ku
