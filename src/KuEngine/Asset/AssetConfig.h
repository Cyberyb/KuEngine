// KuEngine 资产配置模块：定义场景、灯光、节点与材质配置，并负责从配置文件加载这些数据。
#pragma once

#include <array>
#include <filesystem>
#include <string>
#include <vector>

#include <glm/vec3.hpp>

namespace ku::asset {

enum class ShadingModel {
    PBR,
    Unlit,
};

enum class AlphaMode {
    Opaque,
    Mask,
    Blend,
};

inline constexpr size_t maximumPointLights = 4;

struct SceneTransform {
    glm::vec3 position{0.0f};
    glm::vec3 rotationEulerDeg{0.0f};
    glm::vec3 scale{1.0f};
};

struct PointLightConfig {
    glm::vec3 position{0.0f};
    glm::vec3 color{1.0f};
    float intensity = 0.0f;
    float range = 1.0f;
};

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
    std::vector<PointLightConfig> pointLights;
};

struct SceneNodeConfig;

struct SceneConfig {
    SceneCameraConfig camera{};
    SceneLightingConfig lighting{};
    std::string environment;
    std::vector<SceneNodeConfig> nodes;
};

struct MaterialConfig {
    struct TextureBindingConfig {
        std::string source;
        std::string colorSpace;
        std::string channelMapping;
        int uvSet = 0;
        bool hasSource = false;
        bool hasColorSpace = false;
        bool hasChannelMapping = false;
        bool hasUvSet = false;
    };

    std::string id;
    std::string version;
    std::string pipeline;
    ShadingModel shadingModel = ShadingModel::PBR;
    bool hasShadingModel = false;
    std::string alphaMode = "OPAQUE";
    bool hasAlphaMode = false;
    bool doubleSided = false;
    bool hasDoubleSided = false;
    float alphaCutoff = 0.5f;
    bool hasAlphaCutoff = false;
    std::array<float, 4> baseColorFactor{1.0f, 1.0f, 1.0f, 1.0f};
    bool hasBaseColorFactor = false;
    float metallicFactor = 1.0f;
    bool hasMetallicFactor = false;
    float roughnessFactor = 1.0f;
    bool hasRoughnessFactor = false;
    float normalScale = 1.0f;
    bool hasNormalScale = false;
    float occlusionStrength = 1.0f;
    bool hasOcclusionStrength = false;
    std::array<float, 3> emissiveFactor{0.0f, 0.0f, 0.0f};
    bool hasEmissiveFactor = false;
    TextureBindingConfig baseColorBinding{};
    TextureBindingConfig normalBinding{};
    TextureBindingConfig metallicRoughnessBinding{};
    TextureBindingConfig occlusionBinding{};
    TextureBindingConfig emissiveBinding{};
    // Legacy combined binding. It maps to both MR and AO only when neither
    // explicit semantic binding is present.
    TextureBindingConfig ormBinding{};
};

struct SceneNodeConfig {
    std::string id;
    std::string model;
    std::string material;
    SceneTransform transform{};
    MaterialConfig materialOverride{};
    bool hasMaterialOverride = false;
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

void mergeMaterialConfig(
    MaterialConfig& base,
    const MaterialConfig& overrides);

} // namespace ku::asset
