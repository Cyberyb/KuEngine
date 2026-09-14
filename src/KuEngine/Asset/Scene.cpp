#include "Scene.h"

#include <KuEngine/Asset/AssetPath.h>

#include <exception>
#include <unordered_map>
#include <utility>

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
    return !mesh.vertices.empty() && !mesh.indices.empty();
}

} // namespace

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
        if (findMesh(instance.mesh) == nullptr) {
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

            candidate.instances.push_back(SceneInstance{node.id, handle});
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
