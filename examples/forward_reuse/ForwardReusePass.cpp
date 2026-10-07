#include "ForwardReusePass.h"

#include <KuEngine/Asset/AssetConfig.h>
#include <KuEngine/Asset/AssetPath.h>
#include <KuEngine/Asset/HDRImage.h>
#include <KuEngine/Core/Log.h>
#include <KuEngine/RHI/CommandList.h>
#include <KuEngine/RHI/ResourceUploader.h>
#include <KuEngine/Render/ForwardCommon.h>
#include <KuEngine/Render/ForwardProgram.h>
#include <KuEngine/Render/ForwardGraphTargets.h>
#include <KuEngine/Render/ForwardRenderer.h>
#include <KuEngine/Render/GpuMesh.h>
#include <KuEngine/Render/GpuModelAsset.h>
#include <KuEngine/Render/PBRResources.h>
#include <KuEngine/Render/RenderGraph.h>
#include <KuEngine/Render/TextureFactory.h>

#include <glm/gtc/matrix_transform.hpp>
#include <imgui.h>

#include <algorithm>
#include <filesystem>
#include <stdexcept>
#include <vector>

namespace ku {
namespace {
std::filesystem::path assetDirectory()
{
    const auto runtime = std::filesystem::current_path() / "resources"
        / "forward_reuse";
    if (std::filesystem::exists(runtime / "forward-reuse.scene.json")) return runtime;
#ifdef KUENGINE_SOURCE_DIR
    return std::filesystem::path(KUENGINE_SOURCE_DIR) / "examples"
        / "forward_reuse" / "assets";
#else
    return runtime;
#endif
}
} // namespace

ForwardReusePass::ForwardReusePass(bool zeroLights, bool backFacePreset)
    : m_zeroLights(zeroLights)
    , m_viewPreset(backFacePreset ? 1 : 0)
{
}
ForwardReusePass::~ForwardReusePass()
{
    // Descriptor sets go first; borrowed environment/material/model assets
    // remain alive until the renderer has released its pools.
    m_renderer.reset();
    m_environment.reset();
    m_materialVariants.clear();
    m_model.reset();
    m_program.reset();
}

void ForwardReusePass::setup(RenderGraphBuilder& builder)
{
    builder.colorAttachment(
        builder.createImage(
            forward_graph_resource::sceneColor,
            m_colorTargetDesc),
        AttachmentLoadPolicy::Clear,
        AttachmentStorePolicy::Store);
    builder.depthAttachment(
        builder.createImage(
            forward_graph_resource::sceneDepth,
            m_depthTargetDesc),
        AttachmentLoadPolicy::Clear,
        AttachmentStorePolicy::DontCare);
}

void ForwardReusePass::initialize(const RenderContext& context)
{
    const ForwardGraphTargets targets = makeForwardGraphTargets(context);
    validateForwardGraphTargetFormats(
        context.device, targets.color.format, targets.depth.format);
    m_colorTargetDesc = targets.color;
    m_depthTargetDesc = targets.depth;

    // Recompilation (including a swapchain format change) rebuilds the
    // format-dependent program. Release dependent descriptor sets and buffers
    // before their layouts, then repopulate a single live fixture state.
    m_renderer.reset();
    m_environment.reset();
    m_materialVariants.clear();
    m_model.reset();
    m_program.reset();
    const std::filesystem::path root = assetDirectory();
    asset::SceneConfig config{};
    std::string error;
    if (!asset::loadSceneConfigFromFile(
            root / "forward-reuse.scene.json", config, &error)) {
        throw std::runtime_error(error);
    }
    asset::SceneLoadDescription description{};
    description.resourcesRoot = root;
    description.config = config;
    if (!asset::SceneLoader::load(description, m_scene, &error)) {
        throw std::runtime_error(error);
    }
    if (m_scene.meshes.size() != 1 || m_scene.instances.size() != 2
        || m_scene.instances[0].mesh != m_scene.instances[1].mesh) {
        throw std::runtime_error(
            "ForwardReuse fixture must contain two instances sharing one MeshHandle");
    }
    if (m_zeroLights) {
        m_scene.lighting.intensity = 0.0f;
        for (auto& point : m_scene.lighting.pointLights) point.intensity = 0.0f;
    }

    ResourceUploader uploader(context.device);
    TextureFactory textureFactory(context.device, uploader);
    m_program = std::make_unique<ForwardProgram>();
    if (!m_program->initialize(context.device, context.colorFormat,
            context.depthFormat, context.depthCompareOp, error)) {
        throw std::runtime_error(error);
    }
    m_model = std::make_unique<GpuModelAsset>();
    const asset::MeshData& mesh = m_scene.meshes.front().mesh;
    if (!m_model->initialize(context.device, uploader, mesh, error)) {
        throw std::runtime_error(error);
    }
    m_variantPlan = buildMaterialGpuVariantPlan(m_scene.instances);
    m_materialVariants.reserve(m_variantPlan.variants.size());
    for (size_t variantIndex = 0;
         variantIndex < m_variantPlan.variants.size(); ++variantIndex) {
        const auto& representative = m_scene.instances[
            m_variantPlan.representativeInstanceIndices[variantIndex]];
        auto materials = std::make_unique<MaterialGpuResources>();
        if (!materials->initialize(
                context.device, textureFactory, mesh,
                representative.hasMaterialOverride
                    ? &representative.materialOverride : nullptr,
                error, m_program->setLayout(forward_set::material))) {
            throw std::runtime_error(error);
        }
        m_materialVariants.push_back(std::move(materials));
    }
    asset::HDRImageData controlledEnvironment{};
    controlledEnvironment.width = 1;
    controlledEnvironment.height = 1;
    controlledEnvironment.rgba32f = {0.04f, 0.05f, 0.08f, 1.0f};
    m_environment = std::make_unique<PBREnvironmentResources>();
    if (!m_environment->initialize(
            context.device, textureFactory, controlledEnvironment, error,
            m_program->setLayout(forward_set::environment))) {
        throw std::runtime_error(error);
    }
    std::vector<ForwardPipelineKey> keys;
    for (const auto& instance : m_scene.instances) {
        for (const auto& subMesh : mesh.subMeshes) {
            const uint32_t materialIndex = subMesh.materialIndex
                    < mesh.materials.size()
                ? subMesh.materialIndex : 0u;
            keys.push_back(pipelineKeyFor(resolveMaterial(
                mesh.materials[materialIndex],
                instance.hasMaterialOverride
                    ? &instance.materialOverride : nullptr)));
        }
    }
    if (!m_program->ensurePipelines(keys, error)) throw std::runtime_error(error);
    m_renderer = std::make_unique<ForwardRenderer>();
    const size_t drawCapacity =
        m_scene.instances.size() * mesh.subMeshes.size();
    if (!m_renderer->initialize(
            context.device, *m_program, drawCapacity, error)) {
        throw std::runtime_error(error);
    }
    KU_INFO(
        "KUENGINE_FORWARD_REUSE_FIXTURE preset={} lights={} uniqueMeshes=1 instances=2 variants={} forwardDraws=8 forwardVertices=42 graphDraws=9 graphVertices=45 sharedMesh=1 semantics=mr-ao-emissive-mask-blend-double-sided",
        m_viewPreset == 1 ? "backface" : "semantics",
        m_zeroLights ? "zero" : "scene",
        m_materialVariants.size());
}

void ForwardReusePass::execute(CommandList& commandList, const FrameData& frame)
{
    if (!m_renderer || !m_renderer->ready()) return;
    const float aspect = frame.viewerLayout.sceneFramebuffer.height > 0
        ? static_cast<float>(frame.viewerLayout.sceneFramebuffer.width)
            / static_cast<float>(frame.viewerLayout.sceneFramebuffer.height)
        : 16.0f / 9.0f;
    const glm::vec3 cameraPosition = m_viewPreset == 1
        ? glm::vec3(0.0f, 0.0f, -3.2f)
        : glm::vec3(0.0f, 0.0f, 3.2f);
    const glm::mat4 view = glm::lookAt(cameraPosition,
        glm::vec3(0.0f), glm::vec3(0.0f,1.0f,0.0f));
    glm::mat4 projection = glm::perspective(glm::radians(55.0f),
        std::max(aspect,0.01f), 0.1f, 20.0f);
    projection[1][1] *= -1.0f;
    ForwardView forwardView{};
    forwardView.viewProjection = projection * view;
    forwardView.inverseViewProjection = glm::inverse(forwardView.viewProjection);
    forwardView.cameraPosition = cameraPosition;
    forwardView.lighting = m_scene.lighting;
    forwardView.environment = m_environment.get();
    forwardView.environmentEnabled = false;
    forwardView.skyboxEnabled = false;
    forwardView.outputGamma = true;

    std::vector<ForwardDraw> draws;
    const asset::MeshData& mesh = m_scene.meshes.front().mesh;
    for (size_t instanceIndex = 0;
         instanceIndex < m_scene.instances.size(); ++instanceIndex) {
        const asset::SceneInstance& instance = m_scene.instances[instanceIndex];
        const uint32_t variantIndex =
            m_variantPlan.instanceVariantIndices[instanceIndex];
        if (variantIndex >= m_materialVariants.size()) {
            throw std::runtime_error(
                "ForwardReuse material variant index is out of range");
        }
        const glm::mat4 world = asset::sceneTransformMatrix(instance.transform);
        for (size_t subMeshIndex = 0; subMeshIndex < mesh.subMeshes.size(); ++subMeshIndex) {
            const auto& subMesh = mesh.subMeshes[subMeshIndex];
            const uint32_t materialIndex = subMesh.materialIndex < mesh.materials.size()
                ? subMesh.materialIndex : 0u;
            ForwardDraw draw{};
            draw.model = m_model.get();
            draw.materials = m_materialVariants[variantIndex].get();
            draw.subMeshIndex = static_cast<uint32_t>(subMeshIndex);
            draw.materialIndex = materialIndex;
            draw.modelMatrix = world;
            draw.boundsCenterWorld = glm::vec3(world * glm::vec4(
                0.5f * (mesh.boundsMin + mesh.boundsMax), 1.0f));
            draw.material = resolveMaterial(mesh.materials[materialIndex],
                instance.hasMaterialOverride ? &instance.materialOverride : nullptr);
            draws.push_back(draw);
        }
    }
    std::string error;
    if (!m_renderer->render(commandList, forwardView, draws, error)) {
        KU_WARN("ForwardReuse render skipped: {}", error);
    }
}

void ForwardReusePass::drawUI()
{
    const char* presets[] = {"Semantic front", "Double-sided back"};
    ImGui::Combo("View Preset", &m_viewPreset, presets, 2);
    ImGui::Text("Shared MeshHandle: yes | Material GPU variants: %u",
        static_cast<uint32_t>(m_materialVariants.size()));
    ImGui::TextWrapped(
        "Front: patterned background exercises independent MR/AO and asymmetric emissive UV; checker holes are MASK; translucent tile overlaps opaque depth.");
    ImGui::TextWrapped(
        "The Unlit instance disables emissive texture sampling through its own descriptor variant; scalar and UV state remains per draw.");
    ImGui::TextWrapped(
        "Back: only the cyan triangles remain visible from their back faces, proving double-sided culling state.");
    const char* names[] = {"PBR instance", "Unlit instance"};
    ImGui::Combo("Instance", &m_selectedInstance, names, 2);
    auto& instance = m_scene.instances[static_cast<size_t>(m_selectedInstance)];
    ImGui::DragFloat3("Position", &instance.transform.position.x, 0.02f);
    ImGui::DragFloat3("Rotation XYZ", &instance.transform.rotationEulerDeg.x, 0.5f);
    if (ImGui::DragFloat3("Scale", &instance.transform.scale.x, 0.01f, 0.01f, 4.0f)) {
        instance.transform.scale = glm::max(instance.transform.scale, glm::vec3(0.01f));
    }
    if (ImGui::ColorEdit4(
            "Base", instance.materialOverride.baseColorFactor.data())) {
        instance.materialOverride.hasBaseColorFactor = true;
    }
    if (ImGui::ColorEdit3(
            "Emissive", instance.materialOverride.emissiveFactor.data())) {
        instance.materialOverride.hasEmissiveFactor = true;
    }
    int shading = instance.materialOverride.shadingModel == asset::ShadingModel::PBR ? 0 : 1;
    if (ImGui::Combo("Shading", &shading, "PBR\0Unlit\0")) {
        instance.materialOverride.shadingModel = shading == 0
            ? asset::ShadingModel::PBR : asset::ShadingModel::Unlit;
        instance.materialOverride.hasShadingModel = true;
    }
    ImGui::SliderFloat("Directional intensity", &m_scene.lighting.intensity, 0.0f, 4.0f);
    if (!m_scene.lighting.pointLights.empty()) {
        ImGui::SliderFloat("Point intensity",
            &m_scene.lighting.pointLights.front().intensity, 0.0f, 8.0f);
    }
    ImGui::TextDisabled(
        "Set both lights to zero: PBR direct response darkens; Unlit remains visible. Blend draws are sorted far-to-near after Opaque/Mask.");
}

std::optional<CommandListStatistics> ForwardReusePass::expectedFrameStatistics() const
{
    if (m_scene.meshes.empty()) return std::nullopt;
    CommandStatisticsAccumulator stats;
    for (size_t instance = 0; instance < m_scene.instances.size(); ++instance) {
        for (const auto& subMesh : m_scene.meshes.front().mesh.subMeshes) {
            if (subMesh.indexCount > 0) stats.recordIndexedDraw(subMesh.indexCount, 1);
        }
    }
    return stats.statistics();
}
} // namespace ku
