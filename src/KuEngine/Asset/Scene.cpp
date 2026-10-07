#include "Scene.h"

#include <algorithm>

#include <KuEngine/Asset/AssetPath.h>

#include <exception>
#include <cmath>
#include <limits>
#include <unordered_map>
#include <utility>

#include <glm/common.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace ku::asset {

namespace {

void setError(std::string* destination, std::string message)
{
    if (destination != nullptr) {
        *destination = std::move(message);
    }
}

bool isDrawable(const MeshData& mesh)
{
    if (mesh.vertices.empty() || mesh.indices.empty()
        || mesh.vertices.size() > std::numeric_limits<uint32_t>::max()
        || mesh.indices.size() > std::numeric_limits<uint32_t>::max()) {
        return false;
    }

    for (const uint32_t index : mesh.indices) {
        if (index >= mesh.vertices.size()) {
            return false;
        }
    }
    bool hasDrawableRange = mesh.subMeshes.empty();
    for (const SubMeshData& subMesh : mesh.subMeshes) {
        const size_t indexStart = static_cast<size_t>(subMesh.indexStart);
        const size_t indexCount = static_cast<size_t>(subMesh.indexCount);
        if (indexStart > mesh.indices.size()
            || indexCount > mesh.indices.size() - indexStart) {
            return false;
        }
        hasDrawableRange = hasDrawableRange || indexCount > 0;
    }
    return hasDrawableRange;
}

bool finiteVec3(const glm::vec3& value)
{
    return std::isfinite(value.x)
        && std::isfinite(value.y)
        && std::isfinite(value.z);
}

} // namespace

bool validSceneTransform(const SceneTransform& transform) noexcept
{
    constexpr float minimumScale = 1.0e-5f;
    return finiteVec3(transform.position)
        && finiteVec3(transform.rotationEulerDeg)
        && finiteVec3(transform.scale)
        && transform.scale.x > minimumScale
        && transform.scale.y > minimumScale
        && transform.scale.z > minimumScale;
}

glm::mat4 sceneTransformMatrix(const SceneTransform& transform) noexcept
{
    glm::mat4 world = glm::translate(glm::mat4(1.0f), transform.position);
    world = glm::rotate(
        world,
        glm::radians(transform.rotationEulerDeg.z),
        glm::vec3(0.0f, 0.0f, 1.0f));
    world = glm::rotate(
        world,
        glm::radians(transform.rotationEulerDeg.y),
        glm::vec3(0.0f, 1.0f, 0.0f));
    world = glm::rotate(
        world,
        glm::radians(transform.rotationEulerDeg.x),
        glm::vec3(1.0f, 0.0f, 0.0f));
    return glm::scale(world, transform.scale);
}

glm::mat3 sceneNormalMatrix(const glm::mat4& world) noexcept
{
    return glm::transpose(glm::inverse(glm::mat3(world)));
}

bool transformBounds(
    const glm::vec3& localMin,
    const glm::vec3& localMax,
    const glm::mat4& world,
    glm::vec3& outMin,
    glm::vec3& outMax) noexcept
{
    if (!finiteVec3(localMin) || !finiteVec3(localMax)
        || glm::any(glm::greaterThan(localMin, localMax))) {
        return false;
    }
    glm::vec3 candidateMin(std::numeric_limits<float>::max());
    glm::vec3 candidateMax(std::numeric_limits<float>::lowest());
    for (uint32_t corner = 0; corner < 8; ++corner) {
        const glm::vec3 local{
            (corner & 1u) != 0 ? localMax.x : localMin.x,
            (corner & 2u) != 0 ? localMax.y : localMin.y,
            (corner & 4u) != 0 ? localMax.z : localMin.z,
        };
        const glm::vec3 transformed = glm::vec3(world * glm::vec4(local, 1.0f));
        if (!finiteVec3(transformed)) {
            return false;
        }
        candidateMin = glm::min(candidateMin, transformed);
        candidateMax = glm::max(candidateMax, transformed);
    }
    outMin = candidateMin;
    outMax = candidateMax;
    return true;
}

void sanitizeSceneLighting(SceneLightingConfig& lighting) noexcept
{
    if (!finiteVec3(lighting.direction)
        || glm::dot(lighting.direction, lighting.direction) < 1.0e-8f) {
        lighting.direction = glm::vec3(0.35f, 1.0f, 0.45f);
    }
    if (!finiteVec3(lighting.color)) {
        lighting.color = glm::vec3(1.0f);
    }
    lighting.color = glm::max(lighting.color, glm::vec3(0.0f));
    lighting.intensity = std::isfinite(lighting.intensity)
        ? std::max(0.0f, lighting.intensity)
        : 0.0f;

    if (lighting.pointLights.size() > maximumPointLights) {
        lighting.pointLights.resize(maximumPointLights);
    }
    for (PointLightConfig& point : lighting.pointLights) {
        if (!finiteVec3(point.position)) {
            point.position = glm::vec3(0.0f);
        }
        if (!finiteVec3(point.color)) {
            point.color = glm::vec3(1.0f);
        }
        point.color = glm::max(point.color, glm::vec3(0.0f));
        point.intensity = std::isfinite(point.intensity)
            ? std::max(0.0f, point.intensity)
            : 0.0f;
        point.range = std::isfinite(point.range)
            ? std::max(1.0e-4f, point.range)
            : 1.0f;
    }
}

const MeshAsset* SceneData::findMesh(MeshHandle handle) const
{
    if (handle == invalidMeshHandle
        || static_cast<size_t>(handle) >= meshes.size()) {
        return nullptr;
    }
    const MeshAsset& candidate = meshes[handle];
    return candidate.handle == handle ? &candidate : nullptr;
}

MeshAsset* SceneData::findMesh(MeshHandle handle)
{
    return const_cast<MeshAsset*>(
        std::as_const(*this).findMesh(handle));
}

bool SceneData::valid() const
{
    for (size_t i = 0; i < meshes.size(); ++i) {
        if (i >= static_cast<size_t>(invalidMeshHandle)
            || meshes[i].handle != static_cast<MeshHandle>(i)
            || meshes[i].sourcePath.empty()
            || !isDrawable(meshes[i].mesh)) {
            return false;
        }
    }
    for (const SceneInstance& instance : instances) {
        if (findMesh(instance.mesh) == nullptr
            || !validSceneTransform(instance.transform)) {
            return false;
        }
    }
    return !meshes.empty() && !instances.empty();
}

bool SceneLoader::load(
    const SceneLoadDescription& description,
    SceneData& outScene,
    std::string* errorMessage,
    ModelLoadFunction modelLoader)
{
    if (errorMessage != nullptr) {
        errorMessage->clear();
    }
    if (!modelLoader) {
        modelLoader = [](const std::filesystem::path& path) {
            return ModelLoader::loadFromFile(path);
        };
    }
    if (description.resourcesRoot.empty()) {
        setError(errorMessage, "Scene resources root is empty");
        return false;
    }
    if (description.config.nodes.empty()) {
        setError(errorMessage, "Scene has no model nodes");
        return false;
    }

    SceneData candidate{};
    candidate.camera = description.config.camera;
    candidate.lighting = description.config.lighting;
    sanitizeSceneLighting(candidate.lighting);
    candidate.environment.sourcePath = resolveAssetPath(
        description.resourcesRoot,
        description.environmentPath);

    std::unordered_map<std::string, MeshHandle> loadedMeshes;
    loadedMeshes.reserve(description.config.nodes.size());
    candidate.instances.reserve(description.config.nodes.size());

    try {
        for (const SceneNodeConfig& node : description.config.nodes) {
            if (node.model.empty()) {
                setError(
                    errorMessage,
                    "Scene node has no model path: " + node.id);
                return false;
            }
            if (!validSceneTransform(node.transform)) {
                setError(
                    errorMessage,
                    "Scene node transform is non-finite or has non-positive scale: "
                        + node.id);
                return false;
            }

            const std::filesystem::path modelPath = resolveAssetPath(
                description.resourcesRoot,
                node.model);
            const std::string key = assetPathKey(modelPath);
            MeshHandle handle = invalidMeshHandle;

            const auto existing = loadedMeshes.find(key);
            if (existing != loadedMeshes.end()) {
                handle = existing->second;
            } else {
                if (candidate.meshes.size()
                    >= static_cast<size_t>(invalidMeshHandle)) {
                    setError(errorMessage, "Scene exceeds MeshHandle capacity");
                    return false;
                }

                MeshData mesh = modelLoader(modelPath);
                if (!isDrawable(mesh)) {
                    setError(
                        errorMessage,
                        "Model contains no drawable mesh: "
                            + modelPath.string());
                    return false;
                }

                handle = static_cast<MeshHandle>(candidate.meshes.size());
                candidate.meshes.push_back(MeshAsset{
                    handle,
                    modelPath,
                    std::move(mesh),
                });
                loadedMeshes.emplace(key, handle);
            }

            candidate.instances.push_back(SceneInstance{
                .id = node.id,
                .mesh = handle,
                .transform = node.transform,
                .materialReference = node.material.empty()
                    ? std::filesystem::path{}
                    : resolveAssetPath(
                        description.resourcesRoot,
                        node.material),
                .materialOverride = node.materialOverride,
                .hasMaterialOverride = node.hasMaterialOverride,
            });
        }
    } catch (const std::exception& error) {
        setError(errorMessage, error.what());
        return false;
    } catch (...) {
        setError(errorMessage, "Unknown model loading failure");
        return false;
    }

    if (!candidate.valid()) {
        setError(errorMessage, "Scene handle graph is invalid");
        return false;
    }

    outScene = std::move(candidate);
    return true;
}

} // namespace ku::asset
