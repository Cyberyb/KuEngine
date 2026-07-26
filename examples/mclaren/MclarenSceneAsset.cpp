#include "MclarenSceneAsset.h"

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
    *this = MclarenSceneAsset{};
    errorMessage.clear();

    m_environmentPath = sourceOrRuntimePath(
        std::filesystem::path("environments") / "hdr" / kDefaultEnvironmentHdr);
    m_scenePath = sourceOrRuntimePath(
        std::filesystem::path("scenes") / "sandbox" / "mclaren-sandbox.scene.json");

    std::vector<std::filesystem::path> modelPaths;
    std::vector<std::filesystem::path> materialPaths;

    asset::SceneConfig sceneConfig{};
    std::string configError;
    if (asset::loadSceneConfigFromFile(m_scenePath, sceneConfig, &configError)) {
        m_sceneConfigUsed = true;
        m_camera = sceneConfig.camera;
        m_lighting = sceneConfig.lighting;

        const std::filesystem::path resourcesRoot =
            asset::findResourcesRoot(m_scenePath);
        if (resourcesRoot.empty()) {
            KU_WARN(
                "MclarenSceneAsset: cannot resolve resources root from scene path: {}",
                m_scenePath.string());
        } else {
            for (const asset::SceneNodeConfig& node : sceneConfig.nodes) {
                if (!node.model.empty()) {
                    const std::filesystem::path modelPath = resourcesRoot / node.model;
                    if (std::filesystem::exists(modelPath)) {
                        modelPaths.push_back(modelPath);
                    } else {
                        KU_WARN(
                            "MclarenSceneAsset: model does not exist: {}",
                            modelPath.string());
                    }
                }

                if (!node.material.empty()) {
                    const std::filesystem::path materialPath =
                        resourcesRoot / node.material;
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
        m_scenePath.clear();
    }

    if (modelPaths.empty()) {
        modelPaths.push_back(sourceOrRuntimePath(
            std::filesystem::path("models") / "props" / "mclaren_765lt.glb"));
    }

    if (!materialPaths.empty()) {
        m_materialPath = materialPaths.front();
        if (asset::loadMaterialConfigFromFile(
                m_materialPath,
                m_materialConfig,
                &configError)) {
            m_materialConfigUsed = true;
            if (m_materialConfig.hasBaseColorFactor) {
                m_globalBaseColorFactor = m_materialConfig.baseColorFactor;
            }
        } else {
            KU_WARN(
                "MclarenSceneAsset: material config fallback to glTF defaults: {}",
                configError);
            m_materialPath.clear();
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

    if (modelPaths.size() == 1) {
        m_modelLabel = modelPaths.front().string();
    } else {
        std::ostringstream label;
        label << "scene (" << modelPaths.size() << " models)";
        m_modelLabel = label.str();
    }

    bool loadedAny = false;
    for (const std::filesystem::path& modelPath : modelPaths) {
        try {
            appendMeshData(asset::ModelLoader::loadFromFile(modelPath), m_mesh);
            loadedAny = true;
        } catch (const std::exception& error) {
            KU_WARN(
                "MclarenSceneAsset: model load failed ({}): {}",
                modelPath.string(),
                error.what());
        }
    }

    if (!loadedAny) {
        errorMessage = "Model load failed: no scene models were loaded";
        return false;
    }

    m_modelCenter = 0.5f * (m_mesh.boundsMin + m_mesh.boundsMax);
    const float radius =
        0.5f * glm::length(m_mesh.boundsMax - m_mesh.boundsMin);
    m_fitScale = radius > 1e-4f ? 1.5f / radius : 1.0f;
    return true;
}

void MclarenSceneAsset::releaseCpuMesh()
{
    m_mesh = asset::MeshData{};
}

} // namespace ku
