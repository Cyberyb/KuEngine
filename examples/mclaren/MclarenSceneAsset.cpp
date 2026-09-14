#include "MclarenSceneAsset.h"

#include <KuEngine/Asset/AssetPath.h>
#include <KuEngine/Core/Log.h>

#include <glm/glm.hpp>

#include <sstream>
#include <vector>

namespace ku {

namespace {

constexpr const char* kDefaultEnvironmentHdr = "citrus_orchard_road_puresky_4k.hdr";

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

void appendMeshData(
    const asset::MeshData& source,
    asset::MeshData& target)
{
    const uint32_t baseVertex = static_cast<uint32_t>(target.vertices.size());
    const uint32_t baseIndex = static_cast<uint32_t>(target.indices.size());
    const uint32_t baseMaterial = static_cast<uint32_t>(target.materials.size());

    target.vertices.insert(
        target.vertices.end(),
        source.vertices.begin(),
        source.vertices.end());
    target.indices.reserve(target.indices.size() + source.indices.size());
    for (const uint32_t index : source.indices) {
        target.indices.push_back(baseVertex + index);
    }

    for (const asset::SubMeshData& subMesh : source.subMeshes) {
        target.subMeshes.push_back(asset::SubMeshData{
            baseIndex + subMesh.indexStart,
            subMesh.indexCount,
            baseMaterial + subMesh.materialIndex,
        });
    }

    target.materials.insert(
        target.materials.end(),
        source.materials.begin(),
        source.materials.end());

    if (baseVertex == 0) {
        target.boundsMin = source.boundsMin;
        target.boundsMax = source.boundsMax;
    } else {
        target.boundsMin = glm::min(target.boundsMin, source.boundsMin);
        target.boundsMax = glm::max(target.boundsMax, source.boundsMax);
    }
}

} // namespace

bool MclarenSceneAsset::load(std::string& errorMessage)
{
    MclarenSceneAsset candidate{};
    errorMessage.clear();

    candidate.m_environmentPath = sourceOrRuntimePath(
        std::filesystem::path("environments") / "hdr" / kDefaultEnvironmentHdr);
    candidate.m_scenePath = sourceOrRuntimePath(
        std::filesystem::path("scenes") / "sandbox" / "mclaren-sandbox.scene.json");

    asset::SceneConfig loadConfig{};
    std::vector<std::filesystem::path> materialPaths;
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

                if (!node.material.empty()) {
                    const std::filesystem::path materialPath =
                        asset::resolveAssetPath(resourcesRoot, node.material);
                    if (std::filesystem::exists(materialPath)) {
                        materialPaths.push_back(materialPath);
                    } else {
                        KU_WARN(
                            "MclarenSceneAsset: material does not exist: {}",
                            materialPath.string());
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
            sourceOrRuntimePath(
                std::filesystem::path("models") / "props" / "mclaren_765lt.glb")
                .string(),
            {},
        });
    }
    if (resourcesRoot.empty()) {
        resourcesRoot = std::filesystem::current_path();
    }

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

    if (!materialPaths.empty()) {
        candidate.m_materialPath = materialPaths.front();
        if (asset::loadMaterialConfigFromFile(
                candidate.m_materialPath,
                candidate.m_materialConfig,
                &configError)) {
            candidate.m_materialConfigUsed = true;
            if (candidate.m_materialConfig.hasBaseColorFactor) {
                candidate.m_globalBaseColorFactor =
                    candidate.m_materialConfig.baseColorFactor;
            }
        } else {
            KU_WARN(
                "MclarenSceneAsset: material config fallback to glTF defaults: {}",
                configError);
            candidate.m_materialPath.clear();
        }

        for (size_t i = 1; i < materialPaths.size(); ++i) {
            if (materialPaths[i] != materialPaths.front()) {
                KU_WARN(
                    "MclarenSceneAsset: multiple material configs found; only the first is used: {}",
                    materialPaths.front().string());
                break;
            }
        }
    }

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

    for (const asset::SceneInstance& instance :
         candidate.m_sceneData.instances) {
        const asset::MeshAsset* meshAsset =
            candidate.m_sceneData.findMesh(instance.mesh);
        if (meshAsset == nullptr) {
            errorMessage = "Scene instance references an invalid mesh handle";
            return false;
        }
        appendMeshData(meshAsset->mesh, candidate.m_mesh);
    }

    if (!asset::HDRImageLoader::loadFromFile(
            candidate.m_sceneData.environment.sourcePath,
            candidate.m_environmentImage,
            &errorMessage)) {
        return false;
    }

    candidate.m_modelCenter =
        0.5f * (candidate.m_mesh.boundsMin + candidate.m_mesh.boundsMax);
    const float radius =
        0.5f * glm::length(
            candidate.m_mesh.boundsMax - candidate.m_mesh.boundsMin);
    candidate.m_fitScale = radius > 1e-4f ? 1.5f / radius : 1.0f;

    *this = std::move(candidate);
    return true;
}

void MclarenSceneAsset::releaseCpuMesh()
{
    m_mesh = asset::MeshData{};
    m_environmentImage = asset::HDRImageData{};
    for (asset::MeshAsset& meshAsset : m_sceneData.meshes) {
        meshAsset.mesh = asset::MeshData{};
    }
}

} // namespace ku
