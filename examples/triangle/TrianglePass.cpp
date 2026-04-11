#include "TrianglePass.h"

#include "RHI/RHIDevice.h"
#include "RHI/RHIShader.h"
#include "RHI/RHIPipeline.h"

#include <spdlog/spdlog.h>
#include <filesystem>

namespace ku {

TrianglePass::TrianglePass() = default;

TrianglePass::~TrianglePass() = default;

void TrianglePass::initialize(RHIDevice& device) {
    KU_INFO("TrianglePass: initializing...");

    // 着色器路径：相对于当前工作目录（运行时在 build/bin）
    // 用户需从 build/bin 目录运行，或将 shaders/ 复制到运行目录
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

    // 创建管线
    GraphicsPipelineDesc desc{};
    desc.shaders.push_back(*m_vertShader);
    desc.shaders.push_back(*m_fragShader);
    desc.colorFormats = { VK_FORMAT_B8G8R8A8_SRGB };
    desc.depthFormat = VK_FORMAT_D32_SFLOAT;
    desc.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    desc.cullMode = VK_CULL_MODE_BACK_BIT;
    desc.depthTest = true;
    desc.depthWrite = true;

    m_pipeline = std::make_unique<RHIPipeline>(device, desc);
    KU_INFO("TrianglePass: initialized");
}

void TrianglePass::execute(CommandList& cmd, const FrameData&) {
    if (!m_pipeline) return;

    // 全屏三角形，不需要顶点缓冲
    m_pipeline->bind(cmd);
    vkCmdDraw(cmd, 3, 1, 0, 0);
}

void TrianglePass::drawUI() {
    ImGui::Text("Triangle Pass");
    ImGui::Separator();
    ImGui::ColorEdit4("Clear Color", m_clearColor.data());
    ImGui::SliderFloat("Rotation Speed", &m_rotationSpeed, 0.0f, 2.0f, "%.1f");
}

void TrianglePass::onResize(uint32_t width, uint32_t height) {
    KU_DEBUG("TrianglePass: resize {}x{}", width, height);
}

} // namespace ku
