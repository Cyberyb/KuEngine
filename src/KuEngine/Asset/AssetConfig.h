#pragma once

#include <array>
#include <filesystem>
#include <string>
#include <vector>

#include <glm/vec3.hpp>

namespace ku::asset {

struct SceneCameraConfig {
    glm::vec3 position{0.0f, 0.0f, 4.0f};
    glm::vec3 target{0.0f, 0.0f, 0.0f};
    glm::vec3 up{0.0f, 1.0f, 0.0f};
    float fovYDeg = 60.0f;
    float nearPlane = 0.1f;
    float farPlane = 200.0f;
};

struct SceneLightingConfig {
    glm::vec3 direction{0.35f, 1.0f, 0.45f};
    glm::vec3 color{1.0f, 1.0f, 1.0f};
    float intensity = 1.0f;
};

struct SceneNodeConfig {
    std::string id;
    std::string model;
    std::string material;
};

struct SceneConfig {
    SceneCameraConfig camera{};
    SceneLightingConfig lighting{};
    std::vector<SceneNodeConfig> nodes;
};

struct MaterialConfig {
    std::array<float, 4> baseColorFactor{1.0f, 1.0f, 1.0f, 1.0f};
    bool hasBaseColorFactor = false;
};

std::filesystem::path findResourcesRoot(const std::filesystem::path& path);

bool loadSceneConfigFromFile(
    const std::filesystem::path& path,
    SceneConfig& outConfig,
    std::string* errorMessage = nullptr);

bool loadMaterialConfigFromFile(
    const std::filesystem::path& path,
    MaterialConfig& outConfig,
    std::string* errorMessage = nullptr);

} // namespace ku::asset
