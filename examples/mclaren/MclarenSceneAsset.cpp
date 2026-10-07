#include "MclarenSceneAsset.h"

#include <KuEngine/Asset/AssetPath.h>
#include <KuEngine/Core/Log.h>

#include <glm/glm.hpp>

#include <cmath>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace ku {

namespace {

constexpr const char* kDefaultEnvironmentHdr = "citrus_orchard_road_puresky_4k.hdr";
constexpr const char* kDefaultModel = "models/props/mclaren_765lt.glb";

std::filesystem::path sourceOrRuntimePath(
    const std::filesystem::path& relativePath)
{
    const std::filesystem::path runtimePath =
        std::filesystem::current_path() / "resources" / relativePath;
    if (std::filesystem::exists(runtimePath)) {
        return runtimePath;
    }

#ifdef KUENGINE_SOURCE_DIR
    const std::filesystem::path sourcePath =
        std::filesystem::path(KUENGINE_SOURCE_DIR) / "resources" / relativePath;
    if (std::filesystem::exists(sourcePath)) {
        return sourcePath;
    }
#endif

    return runtimePath;
}

std::filesystem::path defaultResourcesRoot()
{
    const std::filesystem::path root = asset::findResourcesRoot(
        sourceOrRuntimePath(kDefaultModel));
    return root.empty()
        ? asset::normalizeAssetPath(
            std::filesystem::current_path() / "resources")
        : asset::normalizeAssetPath(root);
}

} // namespace

bool MclarenSceneAsset::load(std::string& errorMessage)
{
    MclarenSceneAsset candidate{};
    errorMessage.clear();

    candidate.m_scenePath = sourceOrRuntimePath(
        std::filesystem::path("scenes") / "sandbox" / "mclaren-sandbox.scene.json");

    asset::SceneConfig loadConfig{};
    std::filesystem::path resourcesRoot;

    asset::SceneConfig sceneConfig{};
    std::string configError;
    if (asset::loadSceneConfigFromFile(
            candidate.m_scenePath,
            sceneConfig,
            &configError)) {
        candidate.m_sceneConfigUsed = true;
        loadConfig.camera = sceneConfig.camera;
        loadConfig.lighting = sceneConfig.lighting;

        resourcesRoot = asset::findResourcesRoot(candidate.m_scenePath);
        if (resourcesRoot.empty()) {
            KU_WARN(
                "MclarenSceneAsset: cannot resolve resources root from scene path: {}",
                candidate.m_scenePath.string());
        } else {
            for (const asset::SceneNodeConfig& node : sceneConfig.nodes) {
                if (!node.model.empty()) {
                    const std::filesystem::path modelPath =
                        asset::resolveAssetPath(resourcesRoot, node.model);
                    if (std::filesystem::exists(modelPath)) {
                        asset::SceneNodeConfig resolvedNode = node;
                        resolvedNode.model = modelPath.string();
                        loadConfig.nodes.push_back(std::move(resolvedNode));
                    } else {
                        KU_WARN(
                            "MclarenSceneAsset: model does not exist: {}",
                            modelPath.string());
                    }
                }

            }
        }
    } else {
        KU_WARN(
            "MclarenSceneAsset: scene config fallback to defaults: {}",
            configError);
        candidate.m_scenePath.clear();
    }

    if (loadConfig.nodes.empty()) {
        loadConfig.nodes.push_back(asset::SceneNodeConfig{
            "mclaren",
            sourceOrRuntimePath(kDefaultModel).string(),
            {},
        });
    }
    if (resourcesRoot.empty()) {
        resourcesRoot = defaultResourcesRoot();
    }
    candidate.m_resourcesRoot = asset::normalizeAssetPath(resourcesRoot);
    candidate.m_environmentPath = sceneConfig.environment.empty()
        ? sourceOrRuntimePath(
            std::filesystem::path("environments") / "hdr"
                / kDefaultEnvironmentHdr)
        : asset::resolveAssetPath(
            candidate.m_resourcesRoot,
            sceneConfig.environment);

    asset::SceneLoadDescription sceneDescription{};
    sceneDescription.resourcesRoot = resourcesRoot;
    sceneDescription.config = loadConfig;
    sceneDescription.environmentPath = candidate.m_environmentPath;
    if (!asset::SceneLoader::load(
            sceneDescription,
            candidate.m_sceneData,
            &errorMessage)) {
        errorMessage = "Model load failed: " + errorMessage;
        return false;
    }

    for (asset::SceneInstance& instance : candidate.m_sceneData.instances) {
        if (instance.materialReference.empty()) {
            continue;
        }
        asset::MaterialConfig referenced{};
        if (!asset::loadMaterialConfigFromFile(
                instance.materialReference, referenced, &configError)) {
            KU_WARN(
                "MclarenSceneAsset: material config fallback to glTF defaults: {}",
                configError);
            continue;
        }
        if (instance.hasMaterialOverride) {
            asset::mergeMaterialConfig(referenced, instance.materialOverride);
        }
        instance.materialOverride = referenced;
        instance.hasMaterialOverride = true;
        if (candidate.m_materialPath.empty()) {
            candidate.m_materialPath = instance.materialReference;
        }
    }

    if (!finalizeModel(candidate, errorMessage)) {
        return false;
    }

    if (!asset::HDRImageLoader::loadFromFile(
            candidate.m_sceneData.environment.sourcePath,
            candidate.m_environmentImage,
            &errorMessage)) {
        return false;
    }

    *this = std::move(candidate);
    return true;
}

bool MclarenSceneAsset::loadModel(
    const std::filesystem::path& modelPath,
    const std::filesystem::path& resourcesRoot,
    std::string& errorMessage)
{
    MclarenSceneAsset candidate{};
    errorMessage.clear();
    candidate.m_resourcesRoot = asset::normalizeAssetPath(resourcesRoot);

    asset::SceneLoadDescription description{};
    description.resourcesRoot = candidate.m_resourcesRoot;
    description.config.nodes.push_back(asset::SceneNodeConfig{
        "replacement-model",
        modelPath.string(),
        {},
    });
    if (!asset::SceneLoader::load(
            description,
            candidate.m_sceneData,
            &errorMessage)) {
        errorMessage = "Model load failed: " + errorMessage;
        return false;
    }
    if (!finalizeModel(candidate, errorMessage)) {
        return false;
    }
    *this = std::move(candidate);
    return true;
}

bool MclarenSceneAsset::calculateModelFit(
    const glm::vec3& boundsMin,
    const glm::vec3& boundsMax,
    glm::vec3& outCenter,
    float& outScale) noexcept
{
    for (int component = 0; component < 3; ++component) {
        if (!std::isfinite(boundsMin[component])
            || !std::isfinite(boundsMax[component])
            || boundsMin[component] > boundsMax[component]) {
            return false;
        }
    }

    const glm::vec3 center = 0.5f * (boundsMin + boundsMax);
    const float radius = 0.5f * glm::length(boundsMax - boundsMin);
    if (!std::isfinite(center.x) || !std::isfinite(center.y)
        || !std::isfinite(center.z) || !std::isfinite(radius)) {
        return false;
    }
    const float scale = radius > 1e-4f ? 1.5f / radius : 1.0f;
    if (!std::isfinite(scale) || scale <= 0.0f) {
        return false;
    }
    outCenter = center;
    outScale = scale;
    return true;
}

bool MclarenSceneAsset::finalizeModel(
    MclarenSceneAsset& candidate,
    std::string& errorMessage)
{
    if (candidate.m_sceneData.instances.size() == 1) {
        const asset::MeshAsset* meshAsset = candidate.m_sceneData.findMesh(
            candidate.m_sceneData.instances.front().mesh);
        candidate.m_modelLabel = meshAsset != nullptr
            ? meshAsset->sourcePath.string()
            : "scene model";
    } else {
        std::ostringstream label;
        label << "scene (" << candidate.m_sceneData.instances.size()
              << " instances, " << candidate.m_sceneData.meshes.size()
              << " unique models)";
        candidate.m_modelLabel = label.str();
    }

    glm::vec3 boundsMin(std::numeric_limits<float>::max());
    glm::vec3 boundsMax(std::numeric_limits<float>::lowest());
    for (const asset::SceneInstance& instance : candidate.m_sceneData.instances) {
        const asset::MeshAsset* meshAsset =
            candidate.m_sceneData.findMesh(instance.mesh);
        if (meshAsset == nullptr) {
            errorMessage = "Scene instance references an invalid mesh handle";
            return false;
        }
        glm::vec3 instanceMin{};
        glm::vec3 instanceMax{};
        if (!asset::transformBounds(
                meshAsset->mesh.boundsMin,
                meshAsset->mesh.boundsMax,
                asset::sceneTransformMatrix(instance.transform),
                instanceMin,
                instanceMax)) {
            errorMessage = "Scene instance bounds are non-finite or invalid";
            return false;
        }
        boundsMin = glm::min(boundsMin, instanceMin);
        boundsMax = glm::max(boundsMax, instanceMax);
    }
    if (!calculateModelFit(
            boundsMin,
            boundsMax,
            candidate.m_modelCenter,
            candidate.m_fitScale)) {
        errorMessage = "Model bounds are non-finite or invalid";
        return false;
    }
    return true;
}

void MclarenSceneAsset::releaseCpuMesh()
{
    m_environmentImage = asset::HDRImageData{};
    for (asset::MeshAsset& meshAsset : m_sceneData.meshes) {
        meshAsset.mesh = asset::MeshData{};
    }
}

} // namespace ku
