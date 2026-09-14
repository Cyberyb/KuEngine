// KuEngine lightweight scene asset: stable mesh handles plus instance metadata.
#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <limits>
#include <string>
#include <vector>

#include <KuEngine/Asset/AssetConfig.h>
#include <KuEngine/Asset/Model.h>

namespace ku::asset {

using MeshHandle = uint32_t;
inline constexpr MeshHandle invalidMeshHandle =
    std::numeric_limits<MeshHandle>::max();

struct MeshAsset {
    MeshHandle handle = invalidMeshHandle;
    std::filesystem::path sourcePath;
    MeshData mesh;
};

struct SceneInstance {
    std::string id;
    MeshHandle mesh = invalidMeshHandle;

    // ModelLoader currently bakes glTF node transforms into MeshData. A future
    // scene-transform field belongs here, but WP01 intentionally has none.
};

struct SceneEnvironmentMetadata {
    std::filesystem::path sourcePath;
};

struct SceneData {
    SceneCameraConfig camera{};
    SceneLightingConfig lighting{};
    SceneEnvironmentMetadata environment{};
    std::vector<MeshAsset> meshes;
    std::vector<SceneInstance> instances;

    [[nodiscard]] const MeshAsset* findMesh(MeshHandle handle) const;
    [[nodiscard]] MeshAsset* findMesh(MeshHandle handle);
    [[nodiscard]] bool valid() const;
};

struct SceneLoadDescription {
    std::filesystem::path resourcesRoot;
    SceneConfig config{};
    std::filesystem::path environmentPath;
};

using ModelLoadFunction =
    std::function<MeshData(const std::filesystem::path&)>;

class SceneLoader {
public:
    // Publishes to outScene only after every referenced model has loaded and
    // the complete handle graph has validated.
    [[nodiscard]] static bool load(
        const SceneLoadDescription& description,
        SceneData& outScene,
        std::string* errorMessage = nullptr,
        ModelLoadFunction modelLoader = {});
};

} // namespace ku::asset
