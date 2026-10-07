// KuEngine public forward-rendering data contract and CPU-only material logic.
#pragma once

#include <array>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include <KuEngine/Asset/AssetConfig.h>
#include <KuEngine/Asset/Model.h>
#include <KuEngine/Asset/Scene.h>
#include <vulkan/vulkan.h>

namespace ku {

class GpuModelAsset;
class PBRMaterialResources;
using MaterialGpuResources = PBRMaterialResources;
class PBREnvironmentResources;

struct ResolvedMaterial {
    asset::ShadingModel shadingModel = asset::ShadingModel::PBR;
    asset::AlphaMode alphaMode = asset::AlphaMode::Opaque;
    bool doubleSided = false;
    float alphaCutoff = 0.5f;
    std::array<float, 4> baseColorFactor{1.0f, 1.0f, 1.0f, 1.0f};
    std::array<float, 3> emissiveFactor{0.0f, 0.0f, 0.0f};
    float metallicFactor = 1.0f;
    float roughnessFactor = 1.0f;
    float normalScale = 1.0f;
    float occlusionStrength = 1.0f;
    std::array<asset::MaterialData::TextureTransform, 5> textureTransforms{};
};

[[nodiscard]] ResolvedMaterial resolveMaterial(
    const asset::MaterialData& base,
    const asset::MaterialConfig* overrides = nullptr);

struct MaterialTextureVariantKey {
    std::string source;
    std::string colorSpace;
    std::string channelMapping;
    bool hasSource = false;
    bool hasColorSpace = false;
    bool hasChannelMapping = false;

    auto operator<=>(const MaterialTextureVariantKey&) const = default;
};

struct MaterialGpuVariantKey {
    asset::MeshHandle mesh = asset::invalidMeshHandle;
    std::array<MaterialTextureVariantKey, 6> textureBindings{};

    auto operator<=>(const MaterialGpuVariantKey&) const = default;
};

struct MaterialGpuVariantPlan {
    // Canonically sorted so variant indices do not depend on instance order.
    std::vector<MaterialGpuVariantKey> variants;
    std::vector<uint32_t> instanceVariantIndices;
    std::vector<size_t> representativeInstanceIndices;
};

[[nodiscard]] MaterialGpuVariantKey materialGpuVariantKey(
    const asset::SceneInstance& instance);
[[nodiscard]] MaterialGpuVariantPlan buildMaterialGpuVariantPlan(
    std::span<const asset::SceneInstance> instances);

struct ForwardPipelineKey {
    asset::ShadingModel shadingModel = asset::ShadingModel::PBR;
    asset::AlphaMode alphaMode = asset::AlphaMode::Opaque;
    bool doubleSided = false;

    auto operator<=>(const ForwardPipelineKey&) const = default;
};

[[nodiscard]] ForwardPipelineKey pipelineKeyFor(
    const ResolvedMaterial& material) noexcept;

struct ForwardSortKey {
    asset::AlphaMode alphaMode = asset::AlphaMode::Opaque;
    float distanceSquared = 0.0f;
    size_t originalIndex = 0;
};

[[nodiscard]] std::vector<size_t> buildForwardDrawOrder(
    std::span<const ForwardSortKey> keys);

struct ForwardDynamicBufferLayout {
    VkDeviceSize stride = 0;
    VkDeviceSize totalSize = 0;
    uint32_t capacity = 0;
};

[[nodiscard]] bool calculateForwardDynamicBufferLayout(
    size_t drawCapacity,
    VkDeviceSize elementSize,
    VkDeviceSize minimumAlignment,
    ForwardDynamicBufferLayout& outLayout) noexcept;

struct alignas(16) ForwardFrameUniforms {
    float viewProjection[16];
    float inverseViewProjection[16];
    float cameraPosition[4];
    float directionalDirectionIntensity[4];
    float directionalColorPointCount[4];
    float pointPositionRange[asset::maximumPointLights][4];
    float pointColorIntensity[asset::maximumPointLights][4];
    float environmentParams[4];
};

struct alignas(16) ForwardDrawUniforms {
    float model[16];
    float normalRows[12];
    float baseColorFactor[4];
    float emissiveFactor[4];
    float materialFactors[4];
    float textureScaleOffset[5][4];
    float textureRotationTexCoord[5][4];
    float flags[4];
};

struct alignas(16) ForwardSkyboxPushConstants {
    float params[4];
};

struct ForwardView {
    glm::mat4 viewProjection{1.0f};
    glm::mat4 inverseViewProjection{1.0f};
    glm::vec3 cameraPosition{0.0f};
    asset::SceneLightingConfig lighting{};
    const PBREnvironmentResources* environment = nullptr;
    bool environmentEnabled = false;
    bool skyboxEnabled = false;
    bool outputGamma = true;
    float environmentIntensity = 1.0f;
    float environmentExposure = 1.0f;
};

struct ForwardDraw {
    const GpuModelAsset* model = nullptr;
    const MaterialGpuResources* materials = nullptr;
    uint32_t subMeshIndex = 0;
    uint32_t materialIndex = 0;
    glm::mat4 modelMatrix{1.0f};
    glm::vec3 boundsCenterWorld{0.0f};
    ResolvedMaterial material{};
    std::array<float, 4> tint{1.0f, 1.0f, 1.0f, 1.0f};
    std::array<bool, 5> textureEnabled{true, true, true, true, true};
};

} // namespace ku
