// KuEngine PBR GPU assets: narrow material and environment ownership boundaries.
#pragma once

#include <cstdint>
#include <memory>
#include <string>
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

struct PBRMaterialBuildPlan {
    uint32_t materialCount = 1;
    std::vector<uint32_t> subMeshMaterialIndices;
};

[[nodiscard]] PBRMaterialBuildPlan buildPBRMaterialPlan(
    const asset::MeshData& mesh);

struct PBRMaterialResourceStats {
    uint32_t texturedBase = 0;
    uint32_t texturedNormal = 0;
    uint32_t texturedOrm = 0;
    uint32_t texturedEmissive = 0;
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
        std::string& errorMessage);
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
        std::unique_ptr<RHITexture>& destination);
    void destroyVulkanHandles();

    std::vector<PBRMaterialBinding> m_bindings;
    std::vector<std::unique_ptr<RHITexture>> m_materialTextures;
    std::unique_ptr<RHITexture> m_fallbackWhite;
    std::unique_ptr<RHITexture> m_fallbackNormal;
    std::unique_ptr<RHITexture> m_fallbackOrm;
    std::unique_ptr<RHITexture> m_fallbackEmissive;
    VkDescriptorSetLayout m_descriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool m_descriptorPool = VK_NULL_HANDLE;
    VkSampler m_sampler = VK_NULL_HANDLE;
    VkDevice m_device = VK_NULL_HANDLE;
    PBRMaterialResourceStats m_stats{};
};

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
        std::string& errorMessage);
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
};

} // namespace ku
