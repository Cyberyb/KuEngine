// KuEngine PBR GPU assets: narrow material and environment ownership boundaries.
#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <vulkan/vulkan.h>

#include <KuEngine/Asset/AssetConfig.h>
#include <KuEngine/Asset/HDRImage.h>
#include <KuEngine/Asset/Model.h>
#include <KuEngine/Render/PBRCommon.h>

namespace ku {

class RHIDevice;
class RHITexture;
class TextureFactory;

namespace pbr_material_binding {
inline constexpr uint32_t baseColor = 0;
inline constexpr uint32_t normal = 1;
inline constexpr uint32_t metallicRoughness = 2;
inline constexpr uint32_t occlusion = 3;
inline constexpr uint32_t emissive = 4;
inline constexpr uint32_t count = 5;
} // namespace pbr_material_binding

namespace pbr_environment_binding {
inline constexpr uint32_t environment = 0;
} // namespace pbr_environment_binding

struct PBRMaterialBuildPlan {
    uint32_t materialCount = 1;
    std::vector<uint32_t> subMeshMaterialIndices;
};

[[nodiscard]] PBRMaterialBuildPlan buildPBRMaterialPlan(
    const asset::MeshData& mesh);

enum class PBROptionalTexturePayload {
    Missing,
    Invalid,
    Ready,
};

[[nodiscard]] PBROptionalTexturePayload classifyOptionalTexturePayload(
    const asset::TextureData& data) noexcept;

struct PBRMaterialTextureSourcePlan {
    // Borrowed from the input material; consume during resource initialization.
    const asset::TextureData* baseColor = nullptr;
    const asset::TextureData* normal = nullptr;
    const asset::TextureData* metallicRoughness = nullptr;
    const asset::TextureData* occlusion = nullptr;
    const asset::TextureData* emissive = nullptr;
    std::array<bool, pbr_material_binding::count> unsupportedOverrides{};
};

[[nodiscard]] bool hasTextureSourceOverride(
    const asset::MaterialConfig::TextureBindingConfig& binding) noexcept;
[[nodiscard]] PBRMaterialTextureSourcePlan buildPBRMaterialTextureSourcePlan(
    const asset::MaterialData& material,
    const asset::MaterialConfig* config);

struct PBRMaterialResourceStats {
    uint32_t texturedBase = 0;
    uint32_t texturedNormal = 0;
    uint32_t texturedMetallicRoughness = 0;
    uint32_t texturedOcclusion = 0;
    uint32_t texturedEmissive = 0;
    uint32_t optionalTextureFallbacks = 0;
};

class PBRMaterialResources {
public:
    PBRMaterialResources();
    ~PBRMaterialResources();

    PBRMaterialResources(const PBRMaterialResources&) = delete;
    PBRMaterialResources& operator=(const PBRMaterialResources&) = delete;

    [[nodiscard]] bool initialize(
        RHIDevice& device,
        TextureFactory& textureFactory,
        const asset::MeshData& mesh,
        const asset::MaterialConfig* config,
        std::string& errorMessage,
        // Borrowed layouts must outlive this resource when provided.
        VkDescriptorSetLayout externalLayout = VK_NULL_HANDLE);
    void reset();

    [[nodiscard]] bool ready() const;
    [[nodiscard]] VkDescriptorSetLayout descriptorSetLayout() const
    {
        return m_descriptorSetLayout;
    }
    [[nodiscard]] const std::vector<PBRMaterialBinding>& bindings() const
    {
        return m_bindings;
    }
    [[nodiscard]] const PBRMaterialBinding& material(uint32_t index) const;
    [[nodiscard]] const PBRMaterialResourceStats& stats() const
    {
        return m_stats;
    }

private:
    [[nodiscard]] bool uploadTexture(
        TextureFactory& textureFactory,
        const asset::TextureData& data,
        VkFormat format,
        std::string_view bindingName,
        std::unique_ptr<RHITexture>& destination);
    void destroyVulkanHandles();

    std::vector<PBRMaterialBinding> m_bindings;
    std::vector<std::unique_ptr<RHITexture>> m_materialTextures;
    std::unique_ptr<RHITexture> m_fallbackWhite;
    std::unique_ptr<RHITexture> m_fallbackNormal;
    std::unique_ptr<RHITexture> m_fallbackMetallicRoughness;
    std::unique_ptr<RHITexture> m_fallbackOcclusion;
    std::unique_ptr<RHITexture> m_fallbackEmissive;
    VkDescriptorSetLayout m_descriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool m_descriptorPool = VK_NULL_HANDLE;
    VkSampler m_sampler = VK_NULL_HANDLE;
    VkDevice m_device = VK_NULL_HANDLE;
    bool m_ownsDescriptorSetLayout = false;
    PBRMaterialResourceStats m_stats{};
};

using MaterialGpuResources = PBRMaterialResources;

class PBREnvironmentResources {
public:
    PBREnvironmentResources();
    ~PBREnvironmentResources();

    PBREnvironmentResources(const PBREnvironmentResources&) = delete;
    PBREnvironmentResources& operator=(const PBREnvironmentResources&) = delete;

    [[nodiscard]] bool initialize(
        RHIDevice& device,
        TextureFactory& textureFactory,
        const asset::HDRImageData& image,
        std::string& errorMessage,
        // Borrowed layouts must outlive this resource when provided.
        VkDescriptorSetLayout externalLayout = VK_NULL_HANDLE);
    void reset();

    [[nodiscard]] bool ready() const;
    [[nodiscard]] VkDescriptorSetLayout descriptorSetLayout() const
    {
        return m_descriptorSetLayout;
    }
    [[nodiscard]] VkDescriptorSet descriptorSet() const
    {
        return m_descriptorSet;
    }

private:
    void destroyVulkanHandles();

    std::unique_ptr<RHITexture> m_texture;
    VkDescriptorSetLayout m_descriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool m_descriptorPool = VK_NULL_HANDLE;
    VkDescriptorSet m_descriptorSet = VK_NULL_HANDLE;
    VkSampler m_sampler = VK_NULL_HANDLE;
    VkDevice m_device = VK_NULL_HANDLE;
    bool m_ownsDescriptorSetLayout = false;
};

} // namespace ku
