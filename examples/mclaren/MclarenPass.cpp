#include "MclarenPass.h"

#include <KuEngine/Core/Log.h>
#include <KuEngine/Render/GpuMesh.h>
#include <KuEngine/Render/PBRRenderer.h>
#include <KuEngine/Render/RenderGraph.h>

#include <glm/glm.hpp>

#include <imgui.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <vector>

namespace ku {

void MclarenPass::setup(RenderGraphBuilder& builder)
{
    const ResourceHandle swapChainColor =
        builder.importExternal(runtime_resource::swapChainColor);
    builder.colorAttachment(swapChainColor);
    if (m_hasDepth) {
        const ResourceHandle sceneDepth =
            builder.importExternal(runtime_resource::sceneDepth);
        builder.depthAttachment(sceneDepth);
    }
}

void MclarenPass::initialize(const RenderContext& context)
{
    KU_INFO("MclarenPass: initializing...");
    RHIDevice& device = context.device;
    m_hasDepth = context.hasDepth();

    m_loadError.clear();
    m_resources.reset();

    if (!m_scene.load(m_loadError)) {
        KU_ERROR("MclarenPass: {}", m_loadError);
        return;
    }

    m_camera.reset(m_scene.camera());
    m_lightDirection = m_scene.lighting().direction;
    m_lightColor = m_scene.lighting().color;
    m_lightIntensity = m_scene.lighting().intensity;
    m_globalBaseColorFactor = m_scene.globalBaseColorFactor();

    m_enableTextureSampling = true;
    m_enableNormalMap = true;
    m_enableOrmMap = true;
    m_enableEnvironmentMap = true;
    m_enableSkybox = true;
    m_flipUvY = true;
    m_enableOutputGamma = true;
    m_environmentIntensity = 0.7f;
    m_environmentExposure = 1.0f;

    if (!m_resources.initialize(
            device,
            m_scene,
            context.colorFormat,
            context.depthFormat,
            context.depthCompareOp,
            m_loadError)) {
        return;
    }
    m_scene.releaseCpuMesh();

    KU_INFO(
        "MclarenPass initialized: model={}, environment={}",
        m_scene.modelLabel(),
        m_scene.environmentPath().string());
}

void MclarenPass::update(const FrameData&)
{
    ImGuiIO& io = ImGui::GetIO();
    m_camera.updateInput(io.WantCaptureMouse, io.MouseWheel);
}

void MclarenPass::execute(
    CommandList& cmd,
    const FrameData& frameData)
{
    if (!m_resources.ready()) {
        return;
    }

    const OrbitViewportRegion region = m_camera.viewportRegion();
    if (region.width <= 0.0f) {
        return;
    }

    VkViewport viewport{};
    viewport.x = region.x;
    viewport.y = 0.0f;
    viewport.width = region.width;
    viewport.height = static_cast<float>(region.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(cmd, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = {
        static_cast<int32_t>(region.scissorX),
        0,
    };
    scissor.extent = {
        region.scissorWidth,
        region.height,
    };
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    const glm::mat4 model = m_camera.modelMatrix(
        m_scene.fitScale(),
        m_scene.modelCenter());
    const OrbitCameraFrame cameraFrame = m_camera.frame();
    const glm::mat4 mvp = cameraFrame.viewProjection * model;
    const glm::mat3 normalMatrix =
        glm::transpose(glm::inverse(glm::mat3(model)));

    PBRPushConstants basePush{};
    std::memcpy(basePush.mvp, &mvp[0][0], sizeof(basePush.mvp));
    for (int column = 0; column < 3; ++column) {
        basePush.normalRows[column * 4 + 0] =
            normalMatrix[column][0];
        basePush.normalRows[column * 4 + 1] =
            normalMatrix[column][1];
        basePush.normalRows[column * 4 + 2] =
            normalMatrix[column][2];
        basePush.normalRows[column * 4 + 3] =
            m_lightColor[column];
    }

    glm::vec3 lightDirection = m_lightDirection;
    if (glm::length(lightDirection) < 1e-4f) {
        lightDirection = glm::vec3(0.35f, 1.0f, 0.45f);
    } else {
        lightDirection = glm::normalize(lightDirection);
    }

    PBRFrameUniforms baseFrame{};
    std::memcpy(
        baseFrame.model,
        &model[0][0],
        sizeof(baseFrame.model));
    baseFrame.cameraPos[0] = cameraFrame.position.x;
    baseFrame.cameraPos[1] = cameraFrame.position.y;
    baseFrame.cameraPos[2] = cameraFrame.position.z;
    baseFrame.cameraPos[3] = 1.0f;
    std::memcpy(
        baseFrame.invViewProj,
        &cameraFrame.inverseViewProjection[0][0],
        sizeof(baseFrame.invViewProj));
    baseFrame.lightDirIntensity[0] = lightDirection.x;
    baseFrame.lightDirIntensity[1] = lightDirection.y;
    baseFrame.lightDirIntensity[2] = lightDirection.z;
    baseFrame.lightDirIntensity[3] =
        std::max(0.0f, m_lightIntensity);
    baseFrame.alphaParams[1] = 0.5f;

    if (!m_resources.writeSkyboxFrame(baseFrame)) {
        KU_WARN("MclarenPass: failed to update frame uniform buffer");
        return;
    }

    if (m_enableSkybox) {
        m_resources.drawSkybox(
            cmd,
            m_environmentExposure,
            m_enableOutputGamma);
    }

    const GpuMesh& mesh = m_resources.mesh();
    const std::vector<PBRMaterialBinding>& materials =
        m_resources.materials();
    const float alphaMode = m_scene.materialConfigUsed()
        ? alphaModeToFlag(m_scene.materialConfig())
        : 0.0f;

    std::vector<PBRDrawItem> drawItems;
    std::vector<PBRPushConstants> perDrawPush;
    std::vector<PBRFrameUniforms> perDrawFrames;
    drawItems.reserve(mesh.subMeshes().size());
    perDrawPush.reserve(mesh.subMeshes().size());
    perDrawFrames.reserve(mesh.subMeshes().size());

    for (const asset::SubMeshData& subMesh : mesh.subMeshes()) {
        size_t materialIndex =
            static_cast<size_t>(subMesh.materialIndex);
        if (materialIndex >= materials.size()) {
            materialIndex = 0;
        }
        const PBRMaterialBinding& material =
            materials[materialIndex];

        drawItems.push_back(PBRDrawItem{
            subMesh.indexStart,
            subMesh.indexCount,
            &material,
        });

        PBRPushConstants push = basePush;
        const std::array<float, 4> combinedFactor{
            material.baseColorFactor[0]
                * m_globalBaseColorFactor[0],
            material.baseColorFactor[1]
                * m_globalBaseColorFactor[1],
            material.baseColorFactor[2]
                * m_globalBaseColorFactor[2],
            material.baseColorFactor[3]
                * m_globalBaseColorFactor[3],
        };
        std::memcpy(
            push.baseColorFactor,
            combinedFactor.data(),
            sizeof(push.baseColorFactor));
        push.materialParams[0] =
            m_enableTextureSampling
                && material.hasBaseColorTexture
            ? 1.0f
            : 0.0f;
        push.materialParams[1] = m_flipUvY ? 1.0f : 0.0f;
        push.materialParams[2] =
            m_enableNormalMap && material.hasNormalTexture
            ? 1.0f
            : 0.0f;
        push.materialParams[3] =
            m_enableOrmMap && material.hasOrmTexture
            ? 1.0f
            : 0.0f;
        push.materialFactors[0] = material.normalScale;
        push.materialFactors[1] = material.metallicFactor;
        push.materialFactors[2] = material.roughnessFactor;
        push.materialFactors[3] = material.occlusionStrength;

        std::memcpy(
            push.baseUvScaleOffset,
            material.baseUvScaleOffset.data(),
            sizeof(push.baseUvScaleOffset));
        std::memcpy(
            push.normalUvScaleOffset,
            material.normalUvScaleOffset.data(),
            sizeof(push.normalUvScaleOffset));
        std::memcpy(
            push.ormUvScaleOffset,
            material.ormUvScaleOffset.data(),
            sizeof(push.ormUvScaleOffset));
        push.uvTransformParams0[0] = material.baseUvRotation;
        push.uvTransformParams0[1] = material.baseTexCoord;
        push.uvTransformParams0[2] = material.normalUvRotation;
        push.uvTransformParams0[3] = material.normalTexCoord;
        push.uvTransformParams1[0] = material.ormUvRotation;
        push.uvTransformParams1[1] = material.ormTexCoord;
        push.uvTransformParams1[2] =
            m_enableOutputGamma ? 1.0f : 0.0f;
        push.uvTransformParams1[3] = m_enableEnvironmentMap
            ? std::max(0.0f, m_environmentIntensity)
                * std::max(0.0f, m_environmentExposure)
            : 0.0f;
        perDrawPush.push_back(push);

        PBRFrameUniforms frame = baseFrame;
        frame.emissiveFactor[0] = material.emissiveFactor[0];
        frame.emissiveFactor[1] = material.emissiveFactor[1];
        frame.emissiveFactor[2] = material.emissiveFactor[2];
        frame.alphaParams[0] = alphaMode;
        frame.alphaParams[1] = m_scene.materialConfigUsed()
            ? m_scene.materialConfig().alphaCutoff
            : 0.5f;
        perDrawFrames.push_back(frame);
    }

    PBRRenderer& renderer = m_resources.pbrRenderer();
    renderer.setDrawItems(std::move(drawItems));
    renderer.setPerDrawPushConstants(std::move(perDrawPush));
    renderer.setPerDrawFrameUniforms(std::move(perDrawFrames));
    renderer.execute(cmd, frameData);
}

void MclarenPass::drawUI()
{
    ImGui::Begin("Mclaren Model Controls");
    drawUIInline();
    ImGui::End();
}

void MclarenPass::drawUIInline()
{
    ImGui::Text("Model: %s", m_scene.modelLabel().c_str());

    if (m_resources.ready()) {
        const GpuMesh& mesh = m_resources.mesh();
        const MclarenResourceStats& stats = m_resources.stats();
        ImGui::Text("Vertices: %u", mesh.vertexCount());
        ImGui::Text("Indices: %u", mesh.indexCount());
        ImGui::Text(
            "SubMeshes: %u",
            static_cast<uint32_t>(mesh.subMeshes().size()));
        ImGui::Text(
            "Textured materials (base/normal/orm/emissive): %u / %u / %u / %u",
            stats.texturedBase,
            stats.texturedNormal,
            stats.texturedOrm,
            stats.texturedEmissive);
    } else {
        ImGui::TextDisabled("GPU resources are not ready");
    }

    ImGui::Checkbox(
        "Enable BaseColor Texture",
        &m_enableTextureSampling);
    ImGui::Checkbox("Enable Normal Map", &m_enableNormalMap);
    ImGui::Checkbox("Enable ORM Map", &m_enableOrmMap);
    ImGui::Checkbox("Flip UV-Y (Vulkan)", &m_flipUvY);
    ImGui::Checkbox(
        "Encode Output Gamma (Linear->sRGB)",
        &m_enableOutputGamma);
    ImGui::ColorEdit4(
        "Global BaseColor Factor",
        m_globalBaseColorFactor.data());

    ImGui::Separator();
    ImGui::Text("Camera");
    ImGui::Text(
        "Camera Distance (Mouse Wheel): %.2f",
        m_camera.distance());
    ImGui::TextDisabled(
        "Use mouse wheel over viewport to zoom");

    float offsetX = m_camera.viewportOffsetX();
    float visibleWidth = m_camera.visibleWidth();
    ImGui::SliderFloat("Viewport Offset X", &offsetX, 0.0f, 1.0f);
    ImGui::SliderFloat("Visible Width", &visibleWidth, 0.0f, 1.0f);
    m_camera.setViewportRegion(offsetX, visibleWidth);

    float fov = m_camera.fovYDegrees();
    float nearPlane = m_camera.nearPlane();
    float farPlane = m_camera.farPlane();
    ImGui::SliderFloat("Camera FOV Y", &fov, 20.0f, 120.0f);
    ImGui::SliderFloat("Camera Near", &nearPlane, 0.01f, 5.0f);
    ImGui::SliderFloat("Camera Far", &farPlane, 5.0f, 500.0f);
    m_camera.setProjection(fov, nearPlane, farPlane);
    ImGui::Text("Yaw: %.2f", m_camera.yaw());
    ImGui::Text("Pitch: %.2f", m_camera.pitch());

    ImGui::Separator();
    ImGui::Text("Lighting");
    ImGui::SliderFloat3(
        "Light Direction",
        &m_lightDirection.x,
        -1.0f,
        1.0f);
    ImGui::ColorEdit3("Light Color", &m_lightColor.x);
    ImGui::SliderFloat(
        "Light Intensity",
        &m_lightIntensity,
        0.0f,
        4.0f);
    ImGui::Checkbox("Enable Skybox", &m_enableSkybox);
    ImGui::Checkbox(
        "Enable Environment Reflections",
        &m_enableEnvironmentMap);
    ImGui::SliderFloat(
        "Environment Exposure",
        &m_environmentExposure,
        0.1f,
        4.0f);
    ImGui::SliderFloat(
        "Environment Intensity",
        &m_environmentIntensity,
        0.0f,
        2.0f);

    ImGui::Separator();
    ImGui::TextWrapped(
        "Scene Config: %s",
        m_scene.sceneConfigUsed()
            ? m_scene.scenePath().string().c_str()
            : "fallback (not found)");
    ImGui::TextWrapped(
        "Material Config: %s",
        m_scene.materialConfigUsed()
            ? m_scene.materialPath().string().c_str()
            : "fallback (not found)");
    ImGui::TextWrapped(
        "Environment HDR: %s",
        m_scene.environmentPath().string().c_str());

    if (!m_loadError.empty()) {
        ImGui::TextColored(
            ImVec4(1.0f, 0.35f, 0.35f, 1.0f),
            "%s",
            m_loadError.c_str());
    }

    if (ImGui::Button("Reset Rotation")) {
        m_camera.resetRotation();
    }
}

void MclarenPass::onResize(uint32_t width, uint32_t height)
{
    m_camera.onResize(width, height);
}

void MclarenPass::addRotation(
    float deltaYaw,
    float deltaPitch)
{
    m_camera.addRotation(deltaYaw, deltaPitch);
}

} // namespace ku
