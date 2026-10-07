// Mclaren CPU scene asset: preserves stable mesh handles and instance metadata.
#pragma once

#include <array>
#include <filesystem>
#include <string>

#include <glm/vec3.hpp>

#include <KuEngine/Asset/AssetConfig.h>
#include <KuEngine/Asset/HDRImage.h>
#include <KuEngine/Asset/Model.h>
#include <KuEngine/Asset/Scene.h>

namespace ku {

class MclarenSceneAsset {
public:
    [[nodiscard]] bool load(std::string& errorMessage);
    [[nodiscard]] bool loadModel(
        const std::filesystem::path& modelPath,
        const std::filesystem::path& resourcesRoot,
        std::string& errorMessage);
    void releaseCpuMesh();

    [[nodiscard]] static bool calculateModelFit(
        const glm::vec3& boundsMin,
        const glm::vec3& boundsMax,
        glm::vec3& outCenter,
        float& outScale) noexcept;

    [[nodiscard]] const asset::SceneData& sceneData() const { return m_sceneData; }
    [[nodiscard]] asset::SceneData& sceneData() { return m_sceneData; }
    [[nodiscard]] const asset::SceneCameraConfig& camera() const
    {
        return m_sceneData.camera;
    }
    [[nodiscard]] const asset::SceneLightingConfig& lighting() const
    {
        return m_sceneData.lighting;
    }
    [[nodiscard]] const asset::HDRImageData& environmentImage() const
    {
        return m_environmentImage;
    }
    [[nodiscard]] bool sceneConfigUsed() const { return m_sceneConfigUsed; }
    [[nodiscard]] const std::filesystem::path& scenePath() const { return m_scenePath; }
    [[nodiscard]] const std::filesystem::path& materialPath() const { return m_materialPath; }
    [[nodiscard]] const std::filesystem::path& environmentPath() const { return m_environmentPath; }
    [[nodiscard]] const std::filesystem::path& resourcesRoot() const
    {
        return m_resourcesRoot;
    }
    [[nodiscard]] const std::string& modelLabel() const { return m_modelLabel; }

    [[nodiscard]] const glm::vec3& modelCenter() const { return m_modelCenter; }
    [[nodiscard]] float fitScale() const { return m_fitScale; }
    [[nodiscard]] const std::array<float, 4>& globalBaseColorFactor() const
    {
        return m_globalBaseColorFactor;
    }

private:
    [[nodiscard]] static bool finalizeModel(
        MclarenSceneAsset& candidate,
        std::string& errorMessage);

    asset::SceneData m_sceneData;
    asset::HDRImageData m_environmentImage;

    std::filesystem::path m_scenePath;
    std::filesystem::path m_materialPath;
    std::filesystem::path m_environmentPath;
    std::filesystem::path m_resourcesRoot;
    std::string m_modelLabel;

    glm::vec3 m_modelCenter{0.0f};
    float m_fitScale = 1.0f;
    std::array<float, 4> m_globalBaseColorFactor{1.0f, 1.0f, 1.0f, 1.0f};
    bool m_sceneConfigUsed = false;
};

} // namespace ku
