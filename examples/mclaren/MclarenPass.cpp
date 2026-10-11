#include "MclarenPass.h"

#include "MclarenSceneAsset.h"

#include <KuEngine/Core/Log.h>
#include <KuEngine/Core/Input.h>
#include <KuEngine/RHI/ResourceUploader.h>
#include <KuEngine/Render/GpuMesh.h>
#include <KuEngine/Render/GpuModelAsset.h>
#include <KuEngine/Render/ForwardCommon.h>
#include <KuEngine/Render/ForwardGraphTargets.h>
#include <KuEngine/Render/ForwardProgram.h>
#include <KuEngine/Render/ForwardRenderer.h>
#include <KuEngine/Render/PBRResources.h>
#include <KuEngine/Render/RenderGraph.h>
#include <KuEngine/Render/TextureFactory.h>

#include <glm/glm.hpp>

#include <imgui.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

namespace ku {

struct MclarenEnvironmentLifetime {
    std::vector<bool> released;
    uint64_t allocate() { released.push_back(false); return released.size(); }
};

struct MclarenEnvironmentState {
    std::filesystem::path path;
    PBREnvironmentResources resources;
    std::shared_ptr<MclarenEnvironmentLifetime> lifetime;
    uint64_t token = 0;
    ~MclarenEnvironmentState() {
        // Record only AFTER all descriptor/texture/sampler ownership is reset.
        resources.reset();
        if (lifetime && token) {
            lifetime->released[static_cast<size_t>(token-1)] = true;
            KU_INFO("KUENGINE_ENVIRONMENT_RELEASED token={} gpu_reset=1", token);
        }
    }
};

struct MclarenModelState {
    struct MeshGpuState {
        asset::MeshHandle handle = asset::invalidMeshHandle;
        GpuModelAsset model;
    };
    struct MaterialVariantGpuState {
        MaterialGpuVariantKey key{};
        MaterialGpuResources resources;
    };
    struct DrawRecord {
        size_t instanceIndex = 0;
        asset::MeshHandle mesh = asset::invalidMeshHandle;
        uint32_t materialVariantIndex = 0;
        uint32_t subMeshIndex = 0;
        uint32_t materialIndex = 0;
        glm::mat4 instanceWorld{1.0f};
        glm::vec3 localBoundsCenter{0.0f};
        ResolvedMaterial material{};
    };

    MclarenSceneAsset scene;
    std::vector<std::unique_ptr<MeshGpuState>> meshes;
    std::vector<std::unique_ptr<MaterialVariantGpuState>> materialVariants;
    std::vector<DrawRecord> draws;

    [[nodiscard]] const MeshGpuState* find(asset::MeshHandle handle) const
    {
        for (const auto& mesh : meshes) {
            if (mesh->handle == handle) return mesh.get();
        }
        return nullptr;
    }

    [[nodiscard]] const MaterialVariantGpuState* materialVariant(
        uint32_t index) const
    {
        return index < materialVariants.size()
            ? materialVariants[index].get() : nullptr;
    }
};

namespace {

void copyDraft(
    std::array<char, 1024>& destination,
    const std::filesystem::path& path)
{
    const std::string text = path.string();
    std::snprintf(destination.data(), destination.size(), "%s", text.c_str());
}

CommandListStatistics modelStatistics(
    const MclarenModelState& state,
    bool includeSkybox)
{
    CommandStatisticsAccumulator expected;
    if (includeSkybox) {
        expected.recordDraw(3, 1);
    }
    for (const MclarenModelState::DrawRecord& draw : state.draws) {
        const MclarenModelState::MeshGpuState* mesh = state.find(draw.mesh);
        if (mesh == nullptr
            || draw.subMeshIndex >= mesh->model.mesh().subMeshes().size()) {
            continue;
        }
        const asset::SubMeshData& subMesh =
            mesh->model.mesh().subMeshes()[draw.subMeshIndex];
        if (subMesh.indexCount > 0) {
            expected.recordIndexedDraw(subMesh.indexCount, 1);
        }
    }
    return expected.statistics();
}

bool buildModelGpuState(
    RHIDevice& device,
    ResourceUploader& uploader,
    TextureFactory& textureFactory,
    VkDescriptorSetLayout materialLayout,
    MclarenModelState& state,
    std::string& errorMessage)
{
    state.meshes.reserve(state.scene.sceneData().meshes.size());
    for (const asset::MeshAsset& source : state.scene.sceneData().meshes) {
        auto gpu = std::make_unique<MclarenModelState::MeshGpuState>();
        gpu->handle = source.handle;
        if (!gpu->model.initialize(device, uploader, source.mesh, errorMessage)) {
            return false;
        }
        state.meshes.push_back(std::move(gpu));
    }

    const std::span<const asset::SceneInstance> instances =
        state.scene.sceneData().instances;
    const MaterialGpuVariantPlan variantPlan =
        buildMaterialGpuVariantPlan(instances);
    state.materialVariants.reserve(variantPlan.variants.size());
    for (size_t variantIndex = 0;
         variantIndex < variantPlan.variants.size(); ++variantIndex) {
        const MaterialGpuVariantKey& key = variantPlan.variants[variantIndex];
        const asset::MeshAsset* source =
            state.scene.sceneData().findMesh(key.mesh);
        if (source == nullptr) {
            errorMessage = "Material variant references a missing mesh";
            return false;
        }
        const asset::SceneInstance& representative =
            instances[variantPlan.representativeInstanceIndices[variantIndex]];
        auto variant =
            std::make_unique<MclarenModelState::MaterialVariantGpuState>();
        variant->key = key;
        const asset::MaterialConfig* variantConfig =
            representative.hasMaterialOverride
            ? &representative.materialOverride : nullptr;
        if (!variant->resources.initialize(
                device, textureFactory, source->mesh, variantConfig,
                errorMessage, materialLayout)) {
            return false;
        }
        state.materialVariants.push_back(std::move(variant));
    }

    for (size_t instanceIndex = 0; instanceIndex < instances.size();
         ++instanceIndex) {
        const asset::SceneInstance& instance = instances[instanceIndex];
        const asset::MeshAsset* source =
            state.scene.sceneData().findMesh(instance.mesh);
        if (source == nullptr) {
            errorMessage = "Scene instance references a missing mesh";
            return false;
        }
        const glm::mat4 world = asset::sceneTransformMatrix(instance.transform);
        const glm::vec3 center =
            0.5f * (source->mesh.boundsMin + source->mesh.boundsMax);
        for (size_t subMeshIndex = 0;
             subMeshIndex < source->mesh.subMeshes.size(); ++subMeshIndex) {
            const asset::SubMeshData& subMesh =
                source->mesh.subMeshes[subMeshIndex];
            const uint32_t materialIndex = subMesh.materialIndex
                    < source->mesh.materials.size()
                ? subMesh.materialIndex : 0u;
            const asset::MaterialData defaultMaterial{};
            const asset::MaterialData& base = source->mesh.materials.empty()
                ? defaultMaterial : source->mesh.materials[materialIndex];
            state.draws.push_back(MclarenModelState::DrawRecord{
                instanceIndex,
                instance.mesh,
                variantPlan.instanceVariantIndices[instanceIndex],
                static_cast<uint32_t>(subMeshIndex),
                materialIndex,
                world,
                center,
                resolveInstanceMaterial(base, instance, materialIndex)});
        }
    }
    return true;
}

std::vector<ForwardPipelineKey> modelPipelineKeys(
    const MclarenModelState& state)
{
    std::vector<ForwardPipelineKey> keys;
    keys.reserve(state.draws.size());
    for (const auto& draw : state.draws) keys.push_back(pipelineKeyFor(draw.material));
    return keys;
}

} // namespace

MclarenPass::MclarenPass(
    std::vector<MclarenStartupReplacement> startupReplacements,
    uint64_t replaceAfterUpdates, bool deferred, std::optional<asset::RenderPath> pathOverride,
    std::filesystem::path scenePath)
    : m_startupReplacements(std::move(startupReplacements))
    , m_replaceAfterUpdates(replaceAfterUpdates)
    , m_deferred(deferred)
    , m_pathOverride(pathOverride ? pathOverride : deferred ? std::optional(asset::RenderPath::Deferred) : std::nullopt)
    , m_explicitScene(std::move(scenePath))
    , m_environmentLifetime(std::make_shared<MclarenEnvironmentLifetime>())
{
}

MclarenPass::~MclarenPass()
{
    resetAssetStates();
}

void MclarenPass::resetAssetStates() noexcept
{
    m_blendRenderer.reset();
    m_backgroundRenderer.reset();
    m_forwardRenderer.reset();
    m_geometryRenderer.reset();
    m_modelState.reset();
    m_environmentState.reset();
    m_forwardProgram.reset();
    m_geometryProgram.reset();
}

void MclarenPass::setup(RenderGraphBuilder&) {}

ForwardRenderer& MclarenPass::sceneRenderer(SceneDrawPhase phase) {
    return phase == SceneDrawPhase::Background ? *m_backgroundRenderer
        : phase == SceneDrawPhase::Blend ? *m_blendRenderer : phase == SceneDrawPhase::Geometry ? *m_geometryRenderer : *m_forwardRenderer;
}

void MclarenPass::initialize(const RenderContext& context)
{
    m_outputAttachmentSrgb = isSrgbAttachmentFormat(context.colorFormat);
    if (m_forwardRenderer && m_forwardRenderer->ready()) return;
    KU_INFO("MclarenPass: initializing...");
    RHIDevice& device = context.device;
    m_device = &device;
    const ForwardGraphTargets targets = makeForwardGraphTargets(context);
    validateForwardGraphTargetFormats(
        context.device, targets.color.format, targets.depth.format);
    m_colorTargetDesc = targets.color;
    m_depthTargetDesc = targets.depth;

    resetAssetStates();
    m_queueError.clear();
    m_nextStartupReplacement = 0;
    m_completedNormalUpdates = 0;

    MclarenSceneAsset initialScene;
    std::string loadError;
    if (!initialScene.load(loadError, m_explicitScene)) {
        KU_ERROR("MclarenPass: {}", loadError);
        throw std::runtime_error(loadError);
    }

    m_camera.reset(initialScene.camera());
    m_lightDirection = initialScene.lighting().direction;
    m_lightColor = initialScene.lighting().color;
    m_lightIntensity = initialScene.lighting().intensity;
    m_pointLights = initialScene.lighting().pointLights;
    if (m_pointLights.empty()) {
        m_pointLights.push_back(asset::PointLightConfig{
            .position = {1.5f, 1.2f, 2.0f},
            .color = {1.0f, 0.55f, 0.3f},
            .intensity = 0.0f,
            .range = 6.0f,
        });
    }
    m_enableTextureSampling = true;
    m_enableNormalMap = true;
    m_enableOrmMap = true;
    m_enableEnvironmentMap = !initialScene.environmentPath().empty();
    m_enableSkybox = !initialScene.environmentPath().empty();
    m_flipUvY = true;
    m_enableOutputGamma = true;
    m_environmentIntensity = 0.7f;
    m_environmentExposure = 1.0f;

    ResourceUploader uploader(device);
    TextureFactory textureFactory(device, uploader);

    auto program = std::make_unique<ForwardProgram>();
    if (!program->initialize(
            device,
            sceneHdrFormat,
            context.depthFormat,
            context.depthCompareOp,
            loadError)) {
        throw std::runtime_error(loadError);
    }

    auto environment = std::make_unique<MclarenEnvironmentState>();
    environment->path = initialScene.environmentPath();
    if (!environment->resources.initialize(
            device,
            textureFactory,
            initialScene.environmentImage(),
            loadError,
            program->setLayout(forward_set::environment))) {
        throw std::runtime_error(loadError);
    }
    environment->lifetime = m_environmentLifetime;
    environment->token = m_environmentLifetime->allocate();
    KU_INFO("KUENGINE_ENVIRONMENT_CREATED token={} fallback={}", environment->token, environment->path.empty());

    auto model = std::make_unique<MclarenModelState>();
    model->scene = std::move(initialScene);
    if (!buildModelGpuState(
            device, uploader, textureFactory,
            program->setLayout(forward_set::material), *model, loadError)) {
        throw std::runtime_error(loadError);
    }
    const std::vector<ForwardPipelineKey> keys = modelPipelineKeys(*model);
    if (!program->ensurePipelines(keys, loadError)) {
        throw std::runtime_error(loadError);
    }
    auto renderer = std::make_unique<ForwardRenderer>();
    auto geometryProgram = std::make_unique<ForwardProgram>();
    if (pathControl().deferredAvailable()) {
        if (!geometryProgram->initializeGeometry(device, context.depthFormat, context.depthCompareOp, loadError))
            throw std::runtime_error(loadError);
        auto geometryKeys = keys;
        std::erase_if(geometryKeys, [](const auto& k) { return k.alphaMode == asset::AlphaMode::Blend; });
        if (!geometryProgram->ensurePipelines(geometryKeys, loadError)) throw std::runtime_error(loadError);
    }
    if (!renderer->initialize(device, *program, model->draws.size(), loadError)) {
        throw std::runtime_error(loadError);
    }
    model->scene.releaseCpuMesh();

    m_forwardProgram = std::move(program);
    m_forwardRenderer = std::move(renderer);
    m_geometryProgram = std::move(geometryProgram);
    if (pathControl().deferredAvailable()) {
        m_geometryRenderer = std::make_unique<ForwardRenderer>();
        if (!m_geometryRenderer->initialize(device,*m_geometryProgram,model->draws.size(),loadError)) throw std::runtime_error(loadError);
    }
    m_blendRenderer = std::make_unique<ForwardRenderer>();
    m_backgroundRenderer = std::make_unique<ForwardRenderer>();
    if (!m_blendRenderer->initialize(device, *m_forwardProgram, model->draws.size(), loadError)
        || !m_backgroundRenderer->initialize(device, *m_forwardProgram, 1, loadError))
        throw std::runtime_error(loadError);
    m_environmentState = std::move(environment);
    m_modelState = std::move(model);
    pathControl().initialize(asset::resolveRenderPath(m_modelState->scene.renderPath(),m_pathOverride));
    m_deferred = pathControl().path() == asset::RenderPath::Deferred;
    const CommandListStatistics expected = modelStatistics(*m_modelState, true);
    m_replacement.initialize(MclarenPublishedAssetState{
        .modelPath = m_modelState->scene.sceneData().meshes.empty()
            ? std::filesystem::path(m_modelState->scene.modelLabel())
            : m_modelState->scene.sceneData().meshes.front().sourcePath,
        .environmentPath = m_environmentState->path,
        .modelGeneration = 1,
        .environmentGeneration = 1,
        .modelIdentity = reinterpret_cast<uintptr_t>(m_modelState.get()),
        .environmentIdentity =
            reinterpret_cast<uintptr_t>(m_environmentState.get()),
        .expectedDraws = expected.drawCalls,
        .expectedVertices = expected.submittedVertices,
        .modelGlobalBaseColorFactor =
            m_modelState->scene.globalBaseColorFactor(),
    });
    if (m_seedPathSmoke) {
        m_camera.addRotation(0.17f,-0.11f);
        m_camera.setProjection(47.f,.2f,300.f);
        m_camera.setMoveSpeed(5.3f);
        m_camera.setMode(ViewerCameraMode::FreeFly);
        m_lightIntensity=.73f;m_lightColor={.7f,.8f,.9f};
        m_environmentIntensity=.6f;m_environmentExposure=1.7f;
        m_enableSkybox=false;m_enableEnvironmentMap=true;
        m_replacement.modelGlobalBaseColorFactor()={.8f,.65f,.9f,1.f};
        KU_INFO("KUENGINE_PATH_NONDEFAULT_STATE mode=FreeFly fov=47 speed=5.3 light=.73 exposure=1.7 sky=0 environment=1 tint=.8,.65,.9");
    }
    copyDraft(m_modelDraft, m_replacement.live().modelPath);
    copyDraft(m_environmentDraft, m_replacement.live().environmentPath);

    KU_INFO(
        "MclarenPass initialized: model={}, environment={}",
        m_modelState->scene.modelLabel(),
        m_environmentState->path.string());
}

void MclarenPass::update(const FrameData& frame)
{
    pathControl().publishPending();
    // Engine calls update only after waitForFrame() has completed. With the
    // configured single frame in flight this is the fence-safe publication
    // point, before command recording. Resize-skipped frames never enter here,
    // so a pending request naturally survives until the next normal update.
    queueStartupReplacement();
    processPendingReplacement();
    processPendingEdit();
    ++m_completedNormalUpdates;

    m_viewerLayout = frame.viewerLayout;
    const ImGuiIO& io = ImGui::GetIO();
    m_camera.updateInput(
        CameraInputSample{
            .viewerLayout = frame.viewerLayout,
            .deltaTime = frame.deltaTime,
            .pointerX = Input::mouseX(),
            .pointerY = Input::mouseY(),
            .pointerDeltaX = Input::mouseDeltaX(),
            .pointerDeltaY = Input::mouseDeltaY(),
            .wheelY = Input::mouseWheelY(),
            .leftDown = Input::isMouseButtonDown(Input::MOUSE_BUTTON_LEFT),
            .leftPressed = Input::isMouseButtonPressed(Input::MOUSE_BUTTON_LEFT),
            .rightDown = Input::isMouseButtonDown(Input::MOUSE_BUTTON_RIGHT),
            .rightPressed = Input::isMouseButtonPressed(Input::MOUSE_BUTTON_RIGHT),
            .moveForward = Input::isKeyDown(Input::KEY_W),
            .moveBackward = Input::isKeyDown(Input::KEY_S),
            .moveLeft = Input::isKeyDown(Input::KEY_A),
            .moveRight = Input::isKeyDown(Input::KEY_D),
            .moveDown = Input::isKeyDown(Input::KEY_Q),
            .moveUp = Input::isKeyDown(Input::KEY_E),
            .windowActive = Input::isActive(),
            .uiCapturesMouse = io.WantCaptureMouse,
            .uiCapturesKeyboard = io.WantCaptureKeyboard,
            .uiWantsTextInput = io.WantTextInput,
            .interactionEpoch = Input::interactionEpoch(),
        });
}

std::unique_ptr<MclarenEnvironmentState>
MclarenPass::buildEnvironmentCandidate(
    const std::filesystem::path& path,
    std::string& errorMessage,
    MclarenReplacementStage& failedStage,
    MclarenReplacementCategory& failedCategory)
{
    auto candidate = std::make_unique<MclarenEnvironmentState>();
    candidate->path = path;

    failedStage = MclarenReplacementStage::CpuDecode;
    failedCategory =
        MclarenReplacementCategory::CpuDecodeCompatibility;
    asset::HDRImageData image;
    if (path.empty()) {
        image.width = image.height = 1;
        image.rgba32f = {0.f, 0.f, 0.f, 1.f};
    } else if (!asset::HDRImageLoader::loadFromFile(path, image, &errorMessage)) {
        return nullptr;
    }

    failedStage = MclarenReplacementStage::GpuUpload;
    failedCategory = MclarenReplacementCategory::GpuUploadAllocation;
    ResourceUploader uploader(*m_device);
    TextureFactory textureFactory(*m_device, uploader);
    if (!candidate->resources.initialize(
            *m_device,
            textureFactory,
            image,
            errorMessage,
            m_forwardProgram->setLayout(forward_set::environment))) {
        return nullptr;
    }
    candidate->lifetime = m_environmentLifetime;
    candidate->token = m_environmentLifetime->allocate();
    KU_INFO("KUENGINE_ENVIRONMENT_CREATED token={} fallback={}", candidate->token, path.empty());
    return candidate;
}

std::unique_ptr<MclarenModelState> MclarenPass::buildModelCandidate(
    const std::filesystem::path& path,
    std::string& errorMessage,
    MclarenReplacementStage& failedStage,
    MclarenReplacementCategory& failedCategory)
{
    auto candidate = std::make_unique<MclarenModelState>();

    failedStage = MclarenReplacementStage::CpuDecode;
    failedCategory =
        MclarenReplacementCategory::CpuDecodeCompatibility;
    if (!candidate->scene.loadModel(
            path,
            m_modelState->scene.resourcesRoot(),
            errorMessage)) {
        return nullptr;
    }

    failedStage = MclarenReplacementStage::GpuUpload;
    failedCategory = MclarenReplacementCategory::GpuUploadAllocation;
    ResourceUploader uploader(*m_device);
    TextureFactory textureFactory(*m_device, uploader);
    if (!buildModelGpuState(
            *m_device, uploader, textureFactory,
            m_forwardProgram->setLayout(forward_set::material),
            *candidate, errorMessage)) {
        return nullptr;
    }
    if (!m_forwardRenderer->ensureCapacity(
            candidate->draws.size(), errorMessage)
        || !m_blendRenderer->ensureCapacity(candidate->draws.size(), errorMessage)) {
        return nullptr;
    }
    if (m_geometryRenderer && !m_geometryRenderer->ensureCapacity(candidate->draws.size(),errorMessage)) return nullptr;
    const std::vector<ForwardPipelineKey> keys = modelPipelineKeys(*candidate);
    if (!m_forwardProgram->ensurePipelines(keys, errorMessage)) {
        return nullptr;
    }
    if (pathControl().deferredAvailable()) {
        auto geometryKeys = keys;
        std::erase_if(geometryKeys, [](const auto& k) { return k.alphaMode == asset::AlphaMode::Blend; });
        if (!m_geometryProgram->ensurePipelines(geometryKeys, errorMessage)) return nullptr;
    }
    candidate->scene.releaseCpuMesh();
    return candidate;
}

void MclarenPass::publishReplacementFailure(
    const MclarenAssetRequest& request,
    MclarenReplacementStage stage,
    MclarenReplacementCategory category,
    const std::string& message)
{
    m_replacement.publishFailure(request, stage, category, message);
    const std::filesystem::path& active =
        request.kind == MclarenAssetKind::Model
        ? m_replacement.live().modelPath
        : m_replacement.live().environmentPath;
    KU_ERROR(
        "KUENGINE_ASSET_REPLACE_FAIL kind={} request={} retained=1 stage={} category={} active={} message={}",
        toString(request.kind),
        request.id,
        toString(stage),
        toString(category),
        active.string(),
        message);
}

void MclarenPass::processPendingReplacement()
{
    const std::optional<MclarenAssetRequest> pending =
        m_replacement.takePending();
    if (!pending.has_value()) {
        return;
    }
    const MclarenAssetRequest& request = *pending;

    std::filesystem::path resolvedPath;
    std::string errorMessage;
    if (request.kind != MclarenAssetKind::ClearEnvironment && !resolveMclarenAssetPath(
            request.kind,
            request.path,
            m_modelState->scene.resourcesRoot(),
            resolvedPath,
            errorMessage)) {
        publishReplacementFailure(
            request,
            MclarenReplacementStage::Input,
            MclarenReplacementCategory::InputPath,
            errorMessage);
        return;
    }

    MclarenReplacementStage failedStage =
        MclarenReplacementStage::CpuDecode;
    MclarenReplacementCategory failedCategory =
        MclarenReplacementCategory::CpuDecodeCompatibility;
    bool committed = false;
    try {
        if (request.kind == MclarenAssetKind::Model) {
            std::unique_ptr<MclarenModelState> candidate =
                buildModelCandidate(
                    resolvedPath,
                    errorMessage,
                    failedStage,
                    failedCategory);
            if (!candidate) {
                if (isDeviceLostError(errorMessage)) {
                    KU_ERROR(
                        "KUENGINE_ASSET_REPLACE_FAIL kind={} request={} retained=0 stage={} category={} fatal=device_lost message={}",
                        toString(request.kind),
                        request.id,
                        toString(failedStage),
                        toString(failedCategory),
                        errorMessage);
                    throw std::runtime_error(errorMessage);
                }
                publishReplacementFailure(
                    request,
                    failedStage,
                    failedCategory,
                    errorMessage);
                return;
            }

            uint32_t optionalFallbackCount = 0;
            for (const auto& variant : candidate->materialVariants) {
                optionalFallbackCount +=
                    variant->resources.stats().optionalTextureFallbacks;
            }
            const bool optionalFallback = optionalFallbackCount > 0;
            const CommandListStatistics expected =
                modelStatistics(*candidate, m_enableSkybox);
            MclarenReplacementController published = m_replacement;
            published.publishSuccess(
                request,
                resolvedPath,
                reinterpret_cast<uintptr_t>(candidate.get()),
                expected.drawCalls,
                expected.submittedVertices,
                optionalFallback,
                candidate->scene.globalBaseColorFactor());
            m_modelState.swap(candidate);
            m_replacement.swap(published);
            m_selectedInstance = 0; m_selectedSlot = 0;
            committed = true;
            KU_INFO(
                "KUENGINE_ASSET_REPLACE_SUCCESS kind={} request={} active={}",
                toString(request.kind),
                request.id,
                resolvedPath.string());
            if (optionalFallback) {
                KU_WARN(
                    "KUENGINE_ASSET_REPLACE_WARNING kind={} request={} category={} count={}",
                    toString(request.kind),
                    request.id,
                    toString(MclarenReplacementCategory::OptionalTextureFallback),
                    optionalFallbackCount);
            }
        } else {
            std::unique_ptr<MclarenEnvironmentState> candidate =
                buildEnvironmentCandidate(
                    resolvedPath,
                    errorMessage,
                    failedStage,
                    failedCategory);
            if (!candidate) {
                if (isDeviceLostError(errorMessage)) {
                    KU_ERROR(
                        "KUENGINE_ASSET_REPLACE_FAIL kind={} request={} retained=0 stage={} category={} fatal=device_lost message={}",
                        toString(request.kind),
                        request.id,
                        toString(failedStage),
                        toString(failedCategory),
                        errorMessage);
                    throw std::runtime_error(errorMessage);
                }
                publishReplacementFailure(
                    request,
                    failedStage,
                    failedCategory,
                    errorMessage);
                return;
            }

            MclarenReplacementController published = m_replacement;
            const bool wasEmpty = m_replacement.live().environmentPath.empty();
            published.publishSuccess(
                request,
                resolvedPath,
                reinterpret_cast<uintptr_t>(candidate.get()),
                m_replacement.live().expectedDraws,
                m_replacement.live().expectedVertices,
                false,
                m_replacement.live().modelGlobalBaseColorFactor);
            m_environmentState.swap(candidate);
            m_replacement.swap(published);
            if (request.kind == MclarenAssetKind::ClearEnvironment) {
                m_enableSkybox = false; m_enableEnvironmentMap = false;
            } else if (wasEmpty) { m_enableSkybox = true; m_enableEnvironmentMap = true; }
            candidate.reset(); // old GPU ownership is destroyed at this fence-safe point
            committed = true;
            KU_INFO(
                "KUENGINE_ASSET_REPLACE_SUCCESS kind={} request={} active={}",
                toString(request.kind),
                request.id,
                resolvedPath.string());
        }
    } catch (const std::exception& error) {
        if (committed || isDeviceLostError(error.what())) {
            throw;
        }
        publishReplacementFailure(
            request,
            failedStage,
            failedCategory,
            error.what());
    }
}

void MclarenPass::queueStartupReplacement()
{
    if (m_completedNormalUpdates < m_replaceAfterUpdates
        || m_replacement.pending()
        || m_editor.pending()
        || m_nextStartupReplacement >= m_startupReplacements.size()) {
        return;
    }
    const MclarenStartupReplacement& startup =
        m_startupReplacements[m_nextStartupReplacement];
    if (m_replacement.queue(startup.kind, startup.path, &m_queueError)) {
        ++m_nextStartupReplacement;
    }
}

const asset::SceneData& MclarenPass::editableScene() const { return m_modelState->scene.sceneData(); }
uint64_t MclarenPass::environmentToken() const { return m_environmentState ? m_environmentState->token : 0; }
bool MclarenPass::environmentTokenReleased(uint64_t token) const {
    return token>0 && token<=m_environmentLifetime->released.size()
        && m_environmentLifetime->released[static_cast<size_t>(token-1)];
}
bool MclarenPass::requestAsset(MclarenAssetKind kind, std::string path) {
    if (m_editor.pending()) { m_queueError = "An instance edit is pending"; return false; }
    return m_replacement.queue(kind, std::move(path), &m_queueError);
}
bool MclarenPass::requestEdit(MclarenSceneEdit edit) {
    if (m_replacement.pending()) { m_queueError = "An asset operation is pending"; return false; }
    return m_editor.queue(std::move(edit), m_queueError);
}
void MclarenPass::processPendingEdit() {
    if (!m_editor.pending() || !m_modelState) return;
    std::vector<MclarenModelState::DrawRecord> candidateDraws;
    const bool published = m_editor.publish(m_modelState->scene.sceneData(),
        [&](const std::vector<asset::SceneInstance>& instances, std::string& error) {
            candidateDraws = m_modelState->draws;
            std::vector<ForwardPipelineKey> keys;
            for (auto& draw : candidateDraws) {
                const auto& instance = instances.at(draw.instanceIndex);
                const auto* mesh = m_modelState->scene.sceneData().findMesh(draw.mesh);
                const asset::MaterialData fallback{};
                draw.instanceWorld = asset::sceneTransformMatrix(instance.transform);
                draw.material = resolveInstanceMaterial(mesh->mesh.materials.empty() ? fallback
                    : mesh->mesh.materials.at(draw.materialIndex), instance, draw.materialIndex);
                keys.push_back(pipelineKeyFor(draw.material));
            }
            if (!m_forwardProgram->ensurePipelines(keys, error)) return false;
            std::erase_if(keys, [](const auto& key) { return key.alphaMode == asset::AlphaMode::Blend; });
            if (m_geometryRenderer && !m_geometryProgram->ensurePipelines(keys, error)) return false;
            return m_forwardRenderer->ensureCapacity(candidateDraws.size(), error)
                && m_blendRenderer->ensureCapacity(candidateDraws.size(), error)
                && m_backgroundRenderer->ensureCapacity(1, error)
                && (!m_geometryRenderer || m_geometryRenderer->ensureCapacity(candidateDraws.size(), error));
        }, m_queueError);
    if (published) {
        m_modelState->draws.swap(candidateDraws);
        KU_INFO("KUENGINE_INSTANCE_EDIT_PUBLISHED generation={} mesh_upload=0 hdr_upload=0", m_editor.generation());
    } else KU_WARN("KUENGINE_INSTANCE_EDIT_FAILED retained=1 message={}", m_queueError);
}

void MclarenPass::execute(
    CommandList& cmd,
    const FrameData& frameData)
{
    (void)frameData;
    (void)cmd;
    if (!m_modelState || !m_environmentState || !m_forwardRenderer
        || !m_forwardRenderer->ready()) {
        return;
    }

    const CameraFrame cameraFrame = m_camera.frame();
    const glm::mat4 rootModel = m_camera.modelMatrix(
        m_modelState->scene.fitScale(),
        m_modelState->scene.modelCenter());

    ForwardView view{};
    view.viewProjection = cameraFrame.viewProjection;
    view.inverseViewProjection = cameraFrame.inverseViewProjection;
    view.cameraPosition = cameraFrame.position;
    view.lighting.direction = m_lightDirection;
    view.lighting.color = m_lightColor;
    view.lighting.intensity = m_lightIntensity;
    view.lighting.pointLights = m_pointLights;
    view.environment = &m_environmentState->resources;
    view.environmentEnabled = m_enableEnvironmentMap;
    view.skyboxEnabled = m_enableSkybox;
    view.outputGamma = m_enableOutputGamma;
    view.environmentIntensity = m_environmentIntensity;
    view.environmentExposure = m_environmentExposure * (m_pathSmokeVariant ? 2.f : 1.f);

    const std::array<float, 4>& tint =
        m_replacement.live().modelGlobalBaseColorFactor;
    std::vector<ForwardDraw> draws;
    draws.reserve(m_modelState->draws.size());
    for (const MclarenModelState::DrawRecord& record : m_modelState->draws) {
        const MclarenModelState::MeshGpuState* mesh =
            m_modelState->find(record.mesh);
        const MclarenModelState::MaterialVariantGpuState* materialVariant =
            m_modelState->materialVariant(record.materialVariantIndex);
        if (mesh == nullptr || materialVariant == nullptr) {
            continue;
        }
        ForwardDraw draw{};
        draw.model = &mesh->model;
        draw.materials = &materialVariant->resources;
        draw.subMeshIndex = record.subMeshIndex;
        draw.materialIndex = record.materialIndex;
        draw.modelMatrix = rootModel * record.instanceWorld;
        draw.boundsCenterWorld = glm::vec3(
            draw.modelMatrix * glm::vec4(record.localBoundsCenter, 1.0f));
        draw.material = record.material;
        if (m_pathSmokeVariant && draw.material.alphaMode == asset::AlphaMode::Blend)
            draw.material.baseColorFactor[3] = 0.f;
        draw.tint = tint;
        draw.textureEnabled = {
            m_enableTextureSampling,
            m_enableNormalMap,
            m_enableOrmMap,
            m_enableOrmMap,
            true};
        if (m_flipUvY) {
            for (auto& transform : draw.material.textureTransforms) {
                transform.scale.y = -transform.scale.y;
                transform.offset.y = 1.0f - transform.offset.y;
            }
        }
        draws.push_back(draw);
    }

    m_snapshot = {view, std::move(draws), frameData.totalTime};
}

std::vector<uint64_t> MclarenPass::pathSmokeState() const
{
    const auto frame=m_camera.frame();
    const float aspect=float(m_viewerLayout.sceneFramebuffer.width)/std::max(1u,m_viewerLayout.sceneFramebuffer.height);
    // Projection changes legitimately with resize/sidebar; its row norms must
    // still agree with the preserved perspective FOV and the current viewport.
    const float f=1.f/std::tan(glm::radians(m_camera.fovYDegrees())*.5f);
    const auto rowNorm=[&](int row){return glm::length(glm::vec3(frame.viewProjection[0][row],frame.viewProjection[1][row],frame.viewProjection[2][row]));};
    if(std::abs(rowNorm(0)-f/aspect)>.0001f || std::abs(rowNorm(1)-f)>.0001f)
        throw std::runtime_error("Camera projection disagrees with preserved FOV/current viewport");
    const auto& live=m_replacement.live();
    std::vector<uint64_t> state{live.modelGeneration,live.environmentGeneration,live.modelIdentity,live.environmentIdentity,static_cast<uint64_t>(m_camera.mode())};
    const auto scalar=[&](const auto& value) {const auto* bytes=reinterpret_cast<const uint8_t*>(&value);for(size_t i=0;i<sizeof(value);++i)state.push_back(bytes[i]);};
    scalar(m_camera.fovYDegrees());scalar(m_camera.moveSpeed());scalar(m_camera.nearPlane());scalar(m_camera.farPlane());scalar(m_camera.distance());scalar(m_camera.yaw());scalar(m_camera.pitch());scalar(m_environmentExposure);
    for(const char c:live.modelPath.string())state.push_back(static_cast<unsigned char>(c));
    state.push_back(0);
    for(const char c:live.environmentPath.string())state.push_back(static_cast<unsigned char>(c));
    return state;
}

void MclarenPass::drawUI()
{
    pathControl().drawUI();
    if (ImGui::CollapsingHeader(
            "Scene",
            ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::TextWrapped(
            "Active Model: %s",
            m_replacement.live().modelPath.string().c_str());
        ImGui::InputText(
            "Model Draft",
            m_modelDraft.data(),
            m_modelDraft.size());
        ImGui::BeginDisabled(m_replacement.pending() || m_editor.pending());
        if (ImGui::Button("Load Model")) {
            (void)requestAsset(
                MclarenAssetKind::Model,
                std::string(m_modelDraft.data()));
        }
        ImGui::EndDisabled();

        if (m_modelState && m_forwardRenderer && m_forwardRenderer->ready()) {
            uint32_t vertices = 0;
            uint32_t indices = 0;
            PBRMaterialResourceStats stats{};
            for (const auto& mesh : m_modelState->meshes) {
                vertices += mesh->model.mesh().vertexCount();
                indices += mesh->model.mesh().indexCount();
            }
            for (const auto& variant : m_modelState->materialVariants) {
                const auto& source = variant->resources.stats();
                stats.texturedBase += source.texturedBase;
                stats.texturedNormal += source.texturedNormal;
                stats.texturedMetallicRoughness += source.texturedMetallicRoughness;
                stats.texturedOcclusion += source.texturedOcclusion;
                stats.texturedEmissive += source.texturedEmissive;
            }
            ImGui::Text("Unique vertices: %u", vertices);
            ImGui::Text("Unique indices: %u", indices);
            ImGui::Text("Material GPU variants: %u",
                static_cast<uint32_t>(m_modelState->materialVariants.size()));
            ImGui::Text("Draws: %u", static_cast<uint32_t>(m_modelState->draws.size()));
            ImGui::Text(
                "Textured materials (base/normal/MR/AO/emissive): %u / %u / %u / %u / %u",
                stats.texturedBase, stats.texturedNormal,
                stats.texturedMetallicRoughness, stats.texturedOcclusion,
                stats.texturedEmissive);
        } else {
            ImGui::TextDisabled("GPU resources are not ready");
        }

        ImGui::TextWrapped(
            "Scene Config: %s",
            m_modelState && m_modelState->scene.sceneConfigUsed()
                ? m_modelState->scene.scenePath().string().c_str()
                : "fallback (not found)");
        ImGui::TextWrapped(
            "Material Config: %s",
            m_modelState && !m_modelState->scene.materialPath().empty()
                ? m_modelState->scene.materialPath().string().c_str()
                : "fallback (not found)");
        ImGui::Text(
            "Replacement Status: %s",
            std::string(m_replacement.status()).c_str());
        ImGui::Text(
            "Status Category: %s",
            toString(m_replacement.statusCategory()));
        if (!m_queueError.empty()) {
            ImGui::TextColored(
                ImVec4(1.0f, 0.35f, 0.35f, 1.0f),
                "%s",
                m_queueError.c_str());
        }
    }

    if (ImGui::CollapsingHeader(
            "Material",
            ImGuiTreeNodeFlags_DefaultOpen)) {
        if (m_modelState && !editableScene().instances.empty()) {
            const auto& scene = editableScene();
            ImGui::BeginDisabled(m_editor.pending() || m_replacement.pending());
            if (ImGui::BeginCombo("Instance", scene.instances[m_selectedInstance].id.c_str())) {
                for (size_t i = 0; i < scene.instances.size(); ++i)
                    if (ImGui::Selectable(scene.instances[i].id.c_str(), i == m_selectedInstance)) {
                        m_selectedInstance = i; m_selectedSlot = 0;
                    }
                ImGui::EndCombo();
            }
            const auto& instance = scene.instances[m_selectedInstance];
            auto transform = instance.transform;
            bool trs = ImGui::DragFloat3("Instance Position", &transform.position.x, .02f);
            trs |= ImGui::DragFloat3("Instance Rotation (deg)", &transform.rotationEulerDeg.x, .5f);
            trs |= ImGui::DragFloat3("Instance Scale", &transform.scale.x, .01f, .001f, 100.f);
            const auto* mesh = scene.findMesh(instance.mesh);
            int slot = static_cast<int>(m_selectedSlot);
            const int count = static_cast<int>(std::max(size_t(1), mesh->mesh.materials.size()));
            if (ImGui::SliderInt("Material Slot", &slot, 0, count - 1)) m_selectedSlot = static_cast<uint32_t>(slot);
            ImGui::Text("Shared MeshHandle: %u", instance.mesh);
            ImGui::TextUnformatted("This instance only; every primitive using the slot changes.");
            for (size_t p = 0; p < mesh->mesh.subMeshes.size(); ++p)
                if (mesh->mesh.subMeshes[p].materialIndex == m_selectedSlot)
                    ImGui::Text("Affected primitive: %u", static_cast<unsigned>(p));
            auto material = editableMaterial(scene, m_selectedInstance, m_selectedSlot);
            int shading = material.shadingModel == asset::ShadingModel::PBR ? 0 : 1;
            bool changed = ImGui::Combo("Shading Model", &shading, "PBR\0Unlit\0");
            material.shadingModel = shading == 0 ? asset::ShadingModel::PBR : asset::ShadingModel::Unlit;
            changed |= ImGui::ColorEdit4("Slot BaseColor", material.baseColor.data());
            changed |= ImGui::DragFloat3("Slot Emissive [0,64]", material.emissive.data(), .02f, 0.f, 64.f);
            ImGui::BeginDisabled(shading == 1);
            changed |= ImGui::SliderFloat("Slot Metallic", &material.metallic, 0.f, 1.f);
            changed |= ImGui::SliderFloat("Slot Roughness", &material.roughness, 0.f, 1.f);
            ImGui::EndDisabled();
            if (shading == 1) ImGui::TextDisabled("Unlit ignores metallic/roughness; their values are retained.");
            if (trs || changed) {
                MclarenSceneEdit edit; edit.instance = m_selectedInstance;
                if (trs) edit.transform = transform;
                if (changed) edit.material = material;
                (void)requestEdit(std::move(edit));
            }
            ImGui::EndDisabled();
            ImGui::Text("Edit generation: %llu", static_cast<unsigned long long>(m_editor.generation()));
        }
        ImGui::Checkbox(
            "Enable BaseColor Texture",
            &m_enableTextureSampling);
        ImGui::Checkbox("Enable Normal Map", &m_enableNormalMap);
        ImGui::Checkbox("Enable MR/AO Maps", &m_enableOrmMap);
        ImGui::Checkbox("Flip UV-Y (Vulkan)", &m_flipUvY);
        ImGui::ColorEdit4(
            "Global BaseColor Factor",
            m_replacement.modelGlobalBaseColorFactor().data());
    }

    if (ImGui::CollapsingHeader(
            "Camera",
            ImGuiTreeNodeFlags_DefaultOpen)) {
        int cameraMode = m_camera.mode() == ViewerCameraMode::OrbitInspect
            ? 0
            : 1;
        constexpr const char* cameraModes[] = {
            "Orbit Inspect",
            "Free Fly",
        };
        if (ImGui::Combo(
                "Mode",
                &cameraMode,
                cameraModes,
                static_cast<int>(std::size(cameraModes)))) {
            m_camera.setMode(
                cameraMode == 0
                    ? ViewerCameraMode::OrbitInspect
                    : ViewerCameraMode::FreeFly);
        }

        ImGui::Text(
            "Scene logical: %.0f x %.0f",
            m_viewerLayout.sceneLogical.width,
            m_viewerLayout.sceneLogical.height);
        ImGui::Text(
            "Scene viewport: %u x %u pixels",
            m_viewerLayout.sceneFramebuffer.width,
            m_viewerLayout.sceneFramebuffer.height);
        ImGui::Text("Projection aspect: %.3f", m_camera.viewerAspect());

        float fov = m_camera.fovYDegrees();
        float nearPlane = m_camera.nearPlane();
        float farPlane = m_camera.farPlane();
        ImGui::SliderFloat("Camera FOV Y", &fov, 20.0f, 120.0f);
        ImGui::SliderFloat("Camera Near", &nearPlane, 0.01f, 5.0f);
        ImGui::SliderFloat("Camera Far", &farPlane, 5.0f, 500.0f);
        m_camera.setProjection(fov, nearPlane, farPlane);

        if (m_camera.mode() == ViewerCameraMode::OrbitInspect) {
            ImGui::Text("Distance: %.2f", m_camera.distance());
            ImGui::Text("Model Yaw: %.2f", m_camera.yaw());
            ImGui::Text("Model Pitch: %.2f", m_camera.pitch());
            ImGui::TextDisabled(
                "Left drag: rotate model | Wheel: zoom");
        } else {
            const glm::vec3& position = m_camera.freePosition();
            ImGui::Text(
                "Position: %.2f, %.2f, %.2f",
                position.x,
                position.y,
                position.z);
            ImGui::Text(
                "View Yaw/Pitch: %.2f / %.2f",
                m_camera.freeYaw(),
                m_camera.freePitch());
            float moveSpeed = m_camera.moveSpeed();
            if (ImGui::InputFloat(
                    "Move Speed",
                    &moveSpeed,
                    0.25f,
                    1.0f,
                    "%.2f")) {
                m_camera.setMoveSpeed(moveSpeed);
            }
            ImGui::TextDisabled(
                "Right click scene to focus/look | WASD move | Q/E down/up");
        }

        if (ImGui::Button("Reset View")) {
            m_camera.resetView();
        }
        ImGui::SameLine();
        if (ImGui::Button("Reset Model Rotation")) {
            m_camera.resetRotation();
        }
    }

    if (ImGui::CollapsingHeader(
            "Lighting",
            ImGuiTreeNodeFlags_DefaultOpen)) {
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
        if (!m_pointLights.empty()) {
            ImGui::DragFloat3(
                "Point Position", &m_pointLights[0].position.x, 0.05f);
            ImGui::ColorEdit3("Point Color", &m_pointLights[0].color.x);
            ImGui::SliderFloat(
                "Point Intensity", &m_pointLights[0].intensity, 0.0f, 20.0f);
            ImGui::SliderFloat(
                "Point Range", &m_pointLights[0].range, 0.1f, 20.0f);
        }
    }

    if (ImGui::CollapsingHeader(
            "Environment",
            ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::TextWrapped(
            "Active HDR: %s",
            m_replacement.live().environmentPath.string().c_str());
        ImGui::InputText(
            "HDR Draft",
            m_environmentDraft.data(),
            m_environmentDraft.size());
        ImGui::BeginDisabled(m_replacement.pending() || m_editor.pending());
        if (ImGui::Button("Load HDR")) {
            (void)requestAsset(
                MclarenAssetKind::Environment,
                std::string(m_environmentDraft.data()));
        }
        ImGui::SameLine();
        if (ImGui::Button("Clear Environment")) (void)requestAsset(MclarenAssetKind::ClearEnvironment);
        ImGui::EndDisabled();
        ImGui::Checkbox("Enable Skybox", &m_enableSkybox);
        ImGui::Checkbox(
            "Enable Environment Reflections",
            &m_enableEnvironmentMap);
        ImGui::BeginDisabled(m_outputAttachmentSrgb);
        ImGui::Checkbox("Display sRGB encoding (UNORM output)", &m_enableOutputGamma);
        ImGui::EndDisabled();
        if (m_outputAttachmentSrgb) ImGui::TextDisabled("sRGB output attachment: hardware encoding is always active.");
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
    }
}

std::optional<CommandListStatistics>
MclarenPass::expectedFrameStatistics() const
{
    return CommandListStatistics{0,0};
}

void MclarenPass::addRotation(
    float deltaYaw,
    float deltaPitch)
{
    m_camera.addRotation(deltaYaw, deltaPitch);
}

} // namespace ku
