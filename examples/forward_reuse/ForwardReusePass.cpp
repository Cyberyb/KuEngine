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
#include <glm/ext/matrix_clip_space.hpp>
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

ForwardReusePass::ForwardReusePass(bool zeroLights, bool backFacePreset, bool deferred, int lightPreset, int fixture, float normalScale, float exposure, bool sky, bool environment, std::optional<asset::RenderPath> pathOverride)
    : m_zeroLights(zeroLights)
    , m_deferred(deferred)
    , m_pathOverride(pathOverride ? pathOverride : deferred ? std::optional(asset::RenderPath::Deferred) : std::nullopt)
    , m_lightPreset(lightPreset)
    , m_viewPreset(backFacePreset ? 1 : 0)
    , m_fixture(fixture), m_normalScale(normalScale), m_exposure(exposure), m_sky(sky), m_environmentEnabled(environment)
{
}
ForwardReusePass::~ForwardReusePass()
{
    // Descriptor sets go first; borrowed environment/material/model assets
    // remain alive until the renderer has released its pools.
    m_renderer.reset();
    m_geometryRenderer.reset();
    m_blendRenderer.reset();
    m_backgroundRenderer.reset();
    m_environment.reset();
    m_materialVariants.clear();
    m_model.reset();
    m_program.reset();
    m_geometryProgram.reset();
}

void ForwardReusePass::setup(RenderGraphBuilder&) {}

ForwardRenderer& ForwardReusePass::sceneRenderer(SceneDrawPhase phase) {
    return phase == SceneDrawPhase::Background ? *m_backgroundRenderer
        : phase == SceneDrawPhase::Blend ? *m_blendRenderer : phase == SceneDrawPhase::Geometry ? *m_geometryRenderer : *m_renderer;
}
void ForwardReusePass::update(const FrameData&) { pathControl().publishPending(); }

void ForwardReusePass::initialize(const RenderContext& context)
{
    if (m_renderer && m_renderer->ready()) return; // fixed HDR program; keep scene on recompile
    const std::filesystem::path root = assetDirectory();
    asset::SceneConfig config{};
    std::string error;
    if (!asset::loadSceneConfigFromFile(
            root / "forward-reuse.scene.json", config, &error)) {
        throw std::runtime_error(error);
    }
    asset::SceneLoadDescription description{};
    if (m_fixture) {
        for (auto& node : config.nodes) { node.model = "hdr-normal-quad.gltf"; node.transform = {}; }
        config.nodes[0].transform.position = {-0.65f,0,0};
        config.nodes[1].transform.position = {0.65f,0,0};
        for (auto& node : config.nodes) node.transform.scale = {1.1f,1.4f,1};
        config.lighting.pointLights.clear();
    }
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
    if (m_fixture == 1 || m_fixture == 3) {
        // Stable, independently transformed shared mesh instances. Near Blend is
        // deliberately inserted before far Blend to exercise object sorting.
        m_scene.instances.clear();
        const auto add = [&](float x,float y,float z,std::array<float,3> color,float alpha,bool blend) {
            asset::SceneInstance instance{};
            instance.id = "hdr-"+std::to_string(m_scene.instances.size());
            instance.mesh = m_scene.meshes.front().handle;
            instance.transform.position = {x,y,z}; instance.transform.scale = {1.8f,1.3f,1};
            instance.hasMaterialOverride = true;
            auto& m = instance.materialOverride;
            m.hasShadingModel = true; m.shadingModel = asset::ShadingModel::Unlit;
            m.hasBaseColorFactor = true; m.baseColorFactor = {0,0,0,alpha};
            m.hasEmissiveFactor = true; m.emissiveFactor = color;
            m.hasAlphaMode = true; m.alphaMode = blend ? "BLEND" : "OPAQUE";
            m_scene.instances.push_back(instance);
        };
        add(-1,.75f,0,{.25f,.5f,1},1,false);
        add(-1,.75f,.2f,{4,2,.5f},.25f,true);
        add(1,.75f,0,{.25f,.5f,1},1,false);
        add(1,.75f,.3f,{0,2,0},.5f,true);
        add(1,.75f,.1f,{2,0,0},.5f,true);
        add(-1,-.75f,0,{.25f,.5f,1},1,false);
        add(-1,-.75f,.4f,{.1f,.2f,.3f},1,false);
        add(-1,-.75f,.2f,{4,2,.5f},.25f,true);
        add(1,-.75f,0,{4,2,.5f},1,false);
        m_scene.lighting.intensity = 0;
        if (m_fixture == 3) for(auto& instance:m_scene.instances) instance.transform.position.z -= 4.f;
    }
    if (m_fixture == 2) for (auto& instance : m_scene.instances) {
        instance.materialOverride.hasNormalScale = true;
        instance.materialOverride.normalScale = m_normalScale;
    }
    if (m_zeroLights) {
        m_scene.lighting.intensity = 0.0f;
        for (auto& point : m_scene.lighting.pointLights) point.intensity = 0.0f;
    }
    if (m_lightPreset == 1)
        for (auto& point : m_scene.lighting.pointLights) point.intensity = 0.0f;
    if (m_lightPreset == 2) m_scene.lighting.intensity = 0.0f;

    ResourceUploader uploader(context.device);
    TextureFactory textureFactory(context.device, uploader);
    m_program = std::make_unique<ForwardProgram>();
    if (!m_program->initialize(context.device, sceneHdrFormat,
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
    controlledEnvironment.rgba32f = m_fixture ? std::vector<float>{2,1,.5f,1} : std::vector<float>{.04f,.05f,.08f,1};
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
    if (pathControl().deferredAvailable()) {
        m_geometryProgram = std::make_unique<ForwardProgram>();
        if (!m_geometryProgram->initializeGeometry(context.device, context.depthFormat, context.depthCompareOp, error))
            throw std::runtime_error(error);
        std::erase_if(keys, [](const auto& key) { return key.alphaMode == asset::AlphaMode::Blend; });
        if (!m_geometryProgram->ensurePipelines(keys, error)) throw std::runtime_error(error);
    }
    m_renderer = std::make_unique<ForwardRenderer>();
    const size_t drawCapacity =
        m_scene.instances.size() * mesh.subMeshes.size();
    if (!m_renderer->initialize(
            context.device, *m_program, drawCapacity, error, context.framesInFlight)) {
        throw std::runtime_error(error);
    }
    if (pathControl().deferredAvailable()) {
        m_geometryRenderer = std::make_unique<ForwardRenderer>();
        if (!m_geometryRenderer->initialize(context.device,*m_geometryProgram,drawCapacity,error, context.framesInFlight)) throw std::runtime_error(error);
    }
    m_blendRenderer = std::make_unique<ForwardRenderer>();
    m_backgroundRenderer = std::make_unique<ForwardRenderer>();
    if (!m_blendRenderer->initialize(context.device, *m_program, drawCapacity, error, context.framesInFlight)
        || !m_backgroundRenderer->initialize(context.device, *m_program, 1, error, context.framesInFlight))
        throw std::runtime_error(error);
    pathControl().initialize(asset::resolveRenderPath(config.renderPath,m_pathOverride));
    m_deferred = pathControl().path() == asset::RenderPath::Deferred;
    if (!m_fixture) KU_INFO(
        "KUENGINE_FORWARD_REUSE_FIXTURE preset={} lights={} uniqueMeshes=1 instances=2 variants={} forwardDraws=8 forwardVertices=42 graphDraws=9 graphVertices=45 sharedMesh=1 semantics=mr-ao-emissive-mask-blend-double-sided",
        m_viewPreset == 1 ? "backface" : "semantics",
        m_zeroLights ? "zero" : "scene",
        m_materialVariants.size());
    if (m_deferred && !m_fixture)
        KU_INFO("KUENGINE_DEFERRED_FIXTURE geometryDraws=6 geometryVertices=30 blendDraws=2 graphDraws=10 graphVertices=48 lights={} modelCodes=empty:0,pbr:1,unlit:2", m_zeroLights ? "zero" : m_lightPreset == 1 ? "directional" : m_lightPreset == 2 ? "point" : "scene");
    if (m_fixture) KU_INFO("KUENGINE_HDR_FIXTURE kind={} path={} sharedMesh=1 instances={} normalScale={} exposure={} sky={} environment={}",m_fixture==1 ? "analytic" : "normal",m_deferred ? "deferred" : "forward",m_scene.instances.size(),m_normalScale,m_exposure,m_sky,m_environmentEnabled);
}

void ForwardReusePass::execute(CommandList& commandList, const FrameData& frame)
{
    (void)commandList;
    if (!m_renderer || !m_renderer->ready()) return;
    const float aspect = frame.viewerLayout.sceneFramebuffer.height > 0
        ? static_cast<float>(frame.viewerLayout.sceneFramebuffer.width)
            / static_cast<float>(frame.viewerLayout.sceneFramebuffer.height)
        : 16.0f / 9.0f;
    const glm::vec3 offset = m_fixture == 3 ? glm::vec3(0,0,-4) : glm::vec3(0);
    const glm::vec3 cameraPosition = (m_viewPreset == 1
        ? glm::vec3(0.0f, 0.0f, -3.2f)
        : glm::vec3(0.0f, 0.0f, 3.2f)) + offset;
    const glm::mat4 view = glm::lookAt(cameraPosition,
        offset, glm::vec3(0.0f,1.0f,0.0f));
    glm::mat4 projection = m_fixture ? glm::orthoRH_ZO(-2.0f,2.0f,-1.5f,1.5f,.1f,20.0f)
        : glm::perspective(glm::radians(55.0f), std::max(aspect,0.01f), 0.1f, 20.0f);
    projection[1][1] *= -1.0f;
    ForwardView forwardView{};
    forwardView.viewProjection = projection * view;
    forwardView.inverseViewProjection = glm::inverse(forwardView.viewProjection);
    forwardView.cameraPosition = cameraPosition;
    forwardView.lighting = m_scene.lighting;
    forwardView.environment = m_environment.get();
    forwardView.environmentEnabled = m_environmentEnabled;
    forwardView.skyboxEnabled = m_sky;
    forwardView.environmentExposure = m_exposure * (m_pathSmokeVariant ? 2.f : 1.f);
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
            if (m_pathSmokeVariant && draw.material.alphaMode == asset::AlphaMode::Blend)
                draw.material.baseColorFactor[3] = 0.f;
            draws.push_back(draw);
        }
    }
    m_snapshot = {forwardView, std::move(draws), frame.totalTime};
}

void ForwardReusePass::drawUI()
{
    pathControl().drawUI();
    if (m_fixture) { ImGui::Text("Controlled HDR/normal fixture %d",m_fixture); return; }
    if (pathControl().path() == asset::RenderPath::Deferred) ImGui::TextWrapped("Deferred: four RGBA16F MRT, shared linear HDR / Forward Blend / Display.");
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
    return CommandListStatistics{0,0}; // scene preparation records no GPU draw
}
} // namespace ku
