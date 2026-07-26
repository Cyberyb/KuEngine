// Mclaren CPU 场景资产：解析场景/材质配置、合并模型并保存渲染初始化元数据。
#pragma once

#include <array>
#include <filesystem>
#include <string>

#include <glm/vec3.hpp>

#include <KuEngine/Asset/AssetConfig.h>
#include <KuEngine/Asset/Model.h>

namespace ku {

class MclarenSceneAsset {
public:
    [[nodiscard]] bool load(std::string& errorMessage);
    void releaseCpuMesh();

    [[nodiscard]] const asset::MeshData& mesh() const { return m_mesh; }
    [[nodiscard]] const asset::SceneCameraConfig& camera() const { return m_camera; }
    [[nodiscard]] const asset::SceneLightingConfig& lighting() const { return m_lighting; }
    [[nodiscard]] const asset::MaterialConfig& materialConfig() const { return m_materialConfig; }

    [[nodiscard]] bool sceneConfigUsed() const { return m_sceneConfigUsed; }
    [[nodiscard]] bool materialConfigUsed() const { return m_materialConfigUsed; }
    [[nodiscard]] const std::filesystem::path& scenePath() const { return m_scenePath; }
    [[nodiscard]] const std::filesystem::path& materialPath() const { return m_materialPath; }
    [[nodiscard]] const std::filesystem::path& environmentPath() const { return m_environmentPath; }
    [[nodiscard]] const std::string& modelLabel() const { return m_modelLabel; }

    [[nodiscard]] const glm::vec3& modelCenter() const { return m_modelCenter; }
    [[nodiscard]] float fitScale() const { return m_fitScale; }
    [[nodiscard]] const std::array<float, 4>& globalBaseColorFactor() const
    {
        return m_globalBaseColorFactor;
    }

private:
    asset::MeshData m_mesh;
    asset::SceneCameraConfig m_camera{};
    asset::SceneLightingConfig m_lighting{};
    asset::MaterialConfig m_materialConfig{};

    std::filesystem::path m_scenePath;
    std::filesystem::path m_materialPath;
    std::filesystem::path m_environmentPath;
    std::string m_modelLabel;

    glm::vec3 m_modelCenter{0.0f};
    float m_fitScale = 1.0f;
    std::array<float, 4> m_globalBaseColorFactor{1.0f, 1.0f, 1.0f, 1.0f};
    bool m_sceneConfigUsed = false;
    bool m_materialConfigUsed = false;
};

} // namespace ku
