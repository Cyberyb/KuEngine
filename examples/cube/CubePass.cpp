#include "CubePass.h"

#include <KuEngine/Core/Input.h>
#include <KuEngine/Core/Log.h>
#include <KuEngine/Render/RenderGraph.h>

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <imgui.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <cstring>

namespace ku {

float adjustCubeCameraDistance(
    float currentDistance,
    double scrollY) noexcept
{
    const float safeDistance = std::isfinite(currentDistance)
        ? currentDistance
        : cubeCameraDefaultDistance;
    if (!std::isfinite(scrollY)) {
        return std::clamp(
            safeDistance,
            cubeCameraMinimumDistance,
            cubeCameraMaximumDistance);
    }

    return std::clamp(
        safeDistance - static_cast<float>(scrollY) * cubeCameraScrollStep,
        cubeCameraMinimumDistance,
        cubeCameraMaximumDistance);
}

CubePass::CubePass() = default;
CubePass::~CubePass() = default;

void CubePass::setup(RenderGraphBuilder& builder)
{
    const ImageHandle swapChainColor = builder.importImage(
        runtime_resource::swapChainColor,
        m_colorTargetDesc);
    builder.colorAttachment(swapChainColor);
}

void CubePass::initialize(const RenderContext& context)
{
    m_colorTargetDesc = runtimeColorImageDesc(context);
    KU_INFO("CubePass: initializing...");
    RHIDevice& device = context.device;

    std::filesystem::path shaderDir = std::filesystem::current_path() / "shaders";
    auto vertPath = shaderDir / "cube.vert.spv";
    auto fragPath = shaderDir / "cube.frag.spv";

    try {
        m_vertShader = std::make_unique<RHIShader>(device, ShaderDesc{
            vertPath, VK_SHADER_STAGE_VERTEX_BIT, "main"});
        m_fragShader = std::make_unique<RHIShader>(device, ShaderDesc{
            fragPath, VK_SHADER_STAGE_FRAGMENT_BIT, "main"});
    } catch (const std::exception& e) {
        KU_ERROR("CubePass shader load failed: {}", e.what());
        throw;
    }

    GraphicsPipelineDesc solidDesc{};
    solidDesc.shaders.push_back(*m_vertShader);
    solidDesc.shaders.push_back(*m_fragShader);
    solidDesc.colorFormats = {context.colorFormat};
    solidDesc.pushConstantRanges = {
        VkPushConstantRange{
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
            0,
            sizeof(PushConstants)}
    };
    solidDesc.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    solidDesc.cullMode = VK_CULL_MODE_BACK_BIT;
    solidDesc.depthTest = false;
    solidDesc.depthWrite = false;

    GraphicsPipelineDesc wireDesc = solidDesc;
    wireDesc.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
    wireDesc.cullMode = VK_CULL_MODE_NONE;

    m_solidPipeline = std::make_unique<RHIPipeline>(device, solidDesc);
    m_wirePipeline = std::make_unique<RHIPipeline>(device, wireDesc);
    KU_INFO("CubePass: initialized");
}

void CubePass::update(const FrameData& frame)
{
    m_aspect = frame.viewerLayout.sceneAspect;
    const ImGuiIO& io = ImGui::GetIO();
    const ScenePointerAction interaction = m_sceneInteraction.update(
        frame.viewerLayout,
        ScenePointerSample{
            .logicalX = Input::mouseX(),
            .logicalY = Input::mouseY(),
            .deltaX = Input::mouseDeltaX(),
            .deltaY = Input::mouseDeltaY(),
            .scrollY = Input::mouseWheelY(),
            .primaryDown = Input::isMouseButtonDown(Input::MOUSE_BUTTON_LEFT),
            .primaryPressed = Input::isMouseButtonPressed(Input::MOUSE_BUTTON_LEFT),
            .windowActive = Input::isActive(),
            .uiCapturesPointer = io.WantCaptureMouse,
            .interactionEpoch = Input::interactionEpoch(),
        });
    applyViewerInteraction(interaction);
}

void CubePass::execute(CommandList& cmd, const FrameData&)
{
    if (!m_solidPipeline || !m_wirePipeline) {
        return;
    }

    glm::mat4 model = glm::rotate(glm::mat4(1.0f), m_yaw, glm::vec3(0.0f, 1.0f, 0.0f));
    model = glm::rotate(model, m_pitch, glm::vec3(1.0f, 0.0f, 0.0f));

    glm::mat4 view = glm::lookAt(
        glm::vec3(0.0f, 0.0f, m_distance),
        glm::vec3(0.0f, 0.0f, 0.0f),
        glm::vec3(0.0f, 1.0f, 0.0f));

    glm::mat4 proj = glm::perspective(glm::radians(60.0f), std::max(m_aspect, 0.01f), 0.1f, 100.0f);
    proj[1][1] *= -1.0f;

    glm::mat4 mvp = proj * view * model;

    PushConstants push{};
    std::memcpy(push.mvp, &mvp[0][0], sizeof(push.mvp));
    std::memcpy(push.color, m_cubeColor.data(), sizeof(push.color));
    push.params[0] = m_wireframeMode ? 1.0f : 0.0f;

    RHIPipeline* activePipeline = m_wireframeMode ? m_wirePipeline.get() : m_solidPipeline.get();
    activePipeline->bind(cmd);
    vkCmdPushConstants(
        cmd,
        activePipeline->layout(),
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        0,
        sizeof(PushConstants),
        &push);

    if (m_wireframeMode) {
        // 12 edges x 2 vertices per edge.
        cmd.draw(24);
    } else {
        // 6 faces x 2 triangles x 3 vertices.
        cmd.draw(36);
    }
}

void CubePass::drawUI()
{
    ImGui::SeparatorText("Appearance");
    ImGui::ColorEdit4("Cube Color", m_cubeColor.data());
    ImGui::Checkbox("Wireframe Mode", &m_wireframeMode);

    ImGui::SeparatorText("Camera");
    ImGui::SliderFloat(
        "Camera Distance",
        &m_distance,
        cubeCameraMinimumDistance,
        cubeCameraMaximumDistance);
    ImGui::Text("Yaw: %.2f", m_yaw);
    ImGui::Text("Pitch: %.2f", m_pitch);
    if (ImGui::Button("Reset Rotation")) {
        m_yaw = 0.0f;
        m_pitch = 0.0f;
    }
}

void CubePass::onResize(uint32_t width, uint32_t height)
{
    KU_DEBUG("CubePass: resize {}x{}", width, height);
}

void CubePass::addRotation(float deltaYaw, float deltaPitch)
{
    m_yaw += deltaYaw;
    m_pitch = std::clamp(m_pitch + deltaPitch, -1.5f, 1.5f);
}

void CubePass::applyViewerInteraction(
    const ScenePointerAction& interaction) noexcept
{
    m_distance = adjustCubeCameraDistance(m_distance, interaction.scrollY);
    if (interaction.rotate) {
        addRotation(
            static_cast<float>(interaction.rotationDeltaX) * 0.01f,
            static_cast<float>(interaction.rotationDeltaY) * 0.01f);
    }
}

} // namespace ku
