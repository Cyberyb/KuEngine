#include "PBRResources.h"

#include <KuEngine/Core/Log.h>
#include <KuEngine/RHI/RHIDevice.h>
#include <KuEngine/RHI/RHITexture.h>
#include <KuEngine/Render/TextureFactory.h>

#include <algorithm>
#include <array>
#include <exception>
#include <limits>
#include <stdexcept>

namespace ku {

PBRMaterialBuildPlan buildPBRMaterialPlan(const asset::MeshData& mesh)
{
    PBRMaterialBuildPlan plan{};
    const size_t sourceCount = mesh.materials.size();
    if (sourceCount > std::numeric_limits<uint32_t>::max() / 4u) {
        throw std::overflow_error("PBR material count exceeds uint32_t");
    }
    plan.materialCount = std::max(
        1u,
        static_cast<uint32_t>(sourceCount));
    plan.subMeshMaterialIndices.reserve(mesh.subMeshes.size());
    for (const asset::SubMeshData& subMesh : mesh.subMeshes) {
        plan.subMeshMaterialIndices.push_back(
            subMesh.materialIndex < plan.materialCount
                ? subMesh.materialIndex
                : 0u);
    }
    return plan;
}

PBRMaterialResources::PBRMaterialResources() = default;

PBRMaterialResources::~PBRMaterialResources()
{
    reset();
}

bool PBRMaterialResources::initialize(
    RHIDevice& device,
    TextureFactory& textureFactory,
    const asset::MeshData& mesh,
    const asset::MaterialConfig* config,
    std::string& errorMessage)
{
    reset();
    errorMessage.clear();
    m_device = device.device();

    try {
        const PBRMaterialBuildPlan plan = buildPBRMaterialPlan(mesh);

        std::array<VkDescriptorSetLayoutBinding, 4> layoutBindings{};
        for (uint32_t i = 0; i < layoutBindings.size(); ++i) {
            layoutBindings[i].binding = i;
            layoutBindings[i].descriptorType =
                VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            layoutBindings[i].descriptorCount = 1;
            layoutBindings[i].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        }
        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType =
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount =
            static_cast<uint32_t>(layoutBindings.size());
        layoutInfo.pBindings = layoutBindings.data();
        VK_CHECK(vkCreateDescriptorSetLayout(
            m_device,
            &layoutInfo,
            nullptr,
            &m_descriptorSetLayout));

        VkDescriptorPoolSize poolSize{};
        poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        poolSize.descriptorCount = plan.materialCount * 4u;
        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.maxSets = plan.materialCount;
        poolInfo.poolSizeCount = 1;
        poolInfo.pPoolSizes = &poolSize;
        VK_CHECK(vkCreateDescriptorPool(
            m_device,
            &poolInfo,
            nullptr,
            &m_descriptorPool));

        const VkPhysicalDeviceProperties& properties = device.properties();
        VkSamplerCreateInfo samplerInfo{};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter = VK_FILTER_LINEAR;
        samplerInfo.minFilter = VK_FILTER_LINEAR;
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.anisotropyEnable =
            device.features().samplerAnisotropy ? VK_TRUE : VK_FALSE;
        samplerInfo.maxAnisotropy = device.features().samplerAnisotropy
            ? std::min(8.0f, properties.limits.maxSamplerAnisotropy)
            : 1.0f;
        samplerInfo.minLod = 0.0f;
        samplerInfo.maxLod = 0.0f;
        samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
        VK_CHECK(vkCreateSampler(
            m_device,
            &samplerInfo,
            nullptr,
            &m_sampler));

        m_fallbackWhite = textureFactory.createSolidColor(
            {255, 255, 255, 255},
            VK_FORMAT_R8G8B8A8_SRGB);
        m_fallbackNormal = textureFactory.createSolidColor(
            {128, 128, 255, 255},
            VK_FORMAT_R8G8B8A8_UNORM);
        m_fallbackOrm = textureFactory.createSolidColor(
            {255, 255, 0, 255},
            VK_FORMAT_R8G8B8A8_UNORM);
        m_fallbackEmissive = textureFactory.createSolidColor(
            {0, 0, 0, 255},
            VK_FORMAT_R8G8B8A8_UNORM);

        std::vector<VkDescriptorSetLayout> layouts(
            plan.materialCount,
            m_descriptorSetLayout);
        std::vector<VkDescriptorSet> descriptorSets(
            plan.materialCount,
            VK_NULL_HANDLE);
        VkDescriptorSetAllocateInfo allocateInfo{};
        allocateInfo.sType =
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocateInfo.descriptorPool = m_descriptorPool;
        allocateInfo.descriptorSetCount = plan.materialCount;
        allocateInfo.pSetLayouts = layouts.data();
        VK_CHECK(vkAllocateDescriptorSets(
            m_device,
            &allocateInfo,
            descriptorSets.data()));

        m_bindings.reserve(plan.materialCount);
        m_materialTextures.reserve(
            static_cast<size_t>(plan.materialCount) * 4u);
        const bool useConfig = config != nullptr;

        for (uint32_t i = 0; i < plan.materialCount; ++i) {
            asset::MaterialData material{};
            if (i < mesh.materials.size()) {
                material = mesh.materials[i];
            }

            const asset::TextureData* baseSource =
                &material.baseColorTexture;
            const asset::TextureData* normalSource =
                &material.normalTexture;
            const asset::TextureData* ormSource = &material.ormTexture;
            const asset::TextureData* emissiveSource =
                &material.emissiveTexture;

            const auto resolveConfiguredSource =
                [&](const asset::MaterialConfig::TextureBindingConfig& binding,
                    const asset::TextureData* current,
                    const char* bindingName) {
                    if (!useConfig || !binding.hasSource) {
                        return current;
                    }
                    const std::string source = toLower(binding.source);
                    if (isDisabledSource(source)) {
                        return static_cast<const asset::TextureData*>(nullptr);
                    }
                    const asset::TextureData* resolved =
                        resolveGltfTexture(binding.source, material);
                    if (resolved == nullptr) {
                        KU_WARN(
                            "PBRMaterialResources: {} source is unsupported: {}",
                            bindingName,
                            binding.source);
                    }
                    return resolved;
                };

            if (config != nullptr) {
                baseSource = resolveConfiguredSource(
                    config->baseColorBinding,
                    baseSource,
                    "baseColor");
                normalSource = resolveConfiguredSource(
                    config->normalBinding,
                    normalSource,
                    "normal");
                ormSource = resolveConfiguredSource(
                    config->ormBinding,
                    ormSource,
                    "orm");
            }

            std::unique_ptr<RHITexture> baseTexture;
            std::unique_ptr<RHITexture> normalTexture;
            std::unique_ptr<RHITexture> ormTexture;
            std::unique_ptr<RHITexture> emissiveTexture;
            const bool hasBase = baseSource != nullptr
                && uploadTexture(
                    textureFactory,
                    *baseSource,
                    formatForBinding(
                        config != nullptr
                            ? config->baseColorBinding
                            : asset::MaterialConfig::TextureBindingConfig{},
                        VK_FORMAT_R8G8B8A8_SRGB,
                        "baseColor"),
                    baseTexture);
            const bool hasNormal = normalSource != nullptr
                && uploadTexture(
                    textureFactory,
                    *normalSource,
                    formatForBinding(
                        config != nullptr
                            ? config->normalBinding
                            : asset::MaterialConfig::TextureBindingConfig{},
                        VK_FORMAT_R8G8B8A8_UNORM,
                        "normal"),
                    normalTexture);
            const bool hasOrm = ormSource != nullptr
                && uploadTexture(
                    textureFactory,
                    *ormSource,
                    formatForBinding(
                        config != nullptr
                            ? config->ormBinding
                            : asset::MaterialConfig::TextureBindingConfig{},
                        VK_FORMAT_R8G8B8A8_UNORM,
                        "orm"),
                    ormTexture);
            const bool hasEmissive = emissiveSource != nullptr
                && uploadTexture(
                    textureFactory,
                    *emissiveSource,
                    VK_FORMAT_R8G8B8A8_SRGB,
                    emissiveTexture);

            VkImageView baseView = m_fallbackWhite->imageView();
            VkImageView normalView = m_fallbackNormal->imageView();
            VkImageView ormView = m_fallbackOrm->imageView();
            VkImageView emissiveView = m_fallbackEmissive->imageView();
            const auto retain =
                [this](
                    std::unique_ptr<RHITexture>& texture,
                    VkImageView& view,
                    uint32_t& counter) {
                    if (texture) {
                        ++counter;
                        view = texture->imageView();
                        m_materialTextures.push_back(std::move(texture));
                    }
                };
            retain(baseTexture, baseView, m_stats.texturedBase);
            retain(normalTexture, normalView, m_stats.texturedNormal);
            retain(ormTexture, ormView, m_stats.texturedOrm);
            retain(
                emissiveTexture,
                emissiveView,
                m_stats.texturedEmissive);

            std::array<VkDescriptorImageInfo, 4> imageInfos{
                VkDescriptorImageInfo{m_sampler, baseView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
                VkDescriptorImageInfo{m_sampler, normalView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
                VkDescriptorImageInfo{m_sampler, ormView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
                VkDescriptorImageInfo{m_sampler, emissiveView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
            };
            std::array<VkWriteDescriptorSet, 4> writes{};
            for (uint32_t bindingIndex = 0;
                 bindingIndex < writes.size();
                 ++bindingIndex) {
                writes[bindingIndex].sType =
                    VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                writes[bindingIndex].dstSet = descriptorSets[i];
                writes[bindingIndex].dstBinding = bindingIndex;
                writes[bindingIndex].descriptorType =
                    VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                writes[bindingIndex].descriptorCount = 1;
                writes[bindingIndex].pImageInfo = &imageInfos[bindingIndex];
            }
            vkUpdateDescriptorSets(
                m_device,
                static_cast<uint32_t>(writes.size()),
                writes.data(),
                0,
                nullptr);

            PBRMaterialBinding binding{};
            binding.baseColorFactor = {
                material.baseColorFactor.r,
                material.baseColorFactor.g,
                material.baseColorFactor.b,
                material.baseColorFactor.a,
            };
            binding.emissiveFactor = {
                material.emissiveFactor.x,
                material.emissiveFactor.y,
                material.emissiveFactor.z,
            };
            binding.metallicFactor = material.metallicFactor;
            binding.roughnessFactor = material.roughnessFactor;
            binding.normalScale = material.normalScale;
            binding.occlusionStrength = material.occlusionStrength;
            binding.baseUvScaleOffset = {
                material.baseColorTransform.scale.x,
                material.baseColorTransform.scale.y,
                material.baseColorTransform.offset.x,
                material.baseColorTransform.offset.y,
            };
            binding.normalUvScaleOffset = {
                material.normalTransform.scale.x,
                material.normalTransform.scale.y,
                material.normalTransform.offset.x,
                material.normalTransform.offset.y,
            };
            binding.ormUvScaleOffset = {
                material.ormTransform.scale.x,
                material.ormTransform.scale.y,
                material.ormTransform.offset.x,
                material.ormTransform.offset.y,
            };
            binding.baseUvRotation = material.baseColorTransform.rotation;
            binding.normalUvRotation = material.normalTransform.rotation;
            binding.ormUvRotation = material.ormTransform.rotation;
            binding.emissiveUvRotation =
                material.emissiveTransform.rotation;
            binding.baseTexCoord =
                clampTexCoordSet(material.baseColorTransform.texCoord);
            binding.normalTexCoord =
                clampTexCoordSet(material.normalTransform.texCoord);
            binding.ormTexCoord =
                clampTexCoordSet(material.ormTransform.texCoord);
            binding.emissiveTexCoord =
                clampTexCoordSet(material.emissiveTransform.texCoord);
            if (config != nullptr && config->baseColorBinding.hasUvSet) {
                binding.baseTexCoord = clampTexCoordSet(
                    static_cast<uint32_t>(config->baseColorBinding.uvSet));
            }
            if (config != nullptr && config->normalBinding.hasUvSet) {
                binding.normalTexCoord = clampTexCoordSet(
                    static_cast<uint32_t>(config->normalBinding.uvSet));
            }
            if (config != nullptr && config->ormBinding.hasUvSet) {
                binding.ormTexCoord = clampTexCoordSet(
                    static_cast<uint32_t>(config->ormBinding.uvSet));
            }
            binding.hasBaseColorTexture = hasBase;
            binding.hasNormalTexture = hasNormal;
            binding.hasOrmTexture = hasOrm;
            binding.hasEmissiveTexture = hasEmissive;
            binding.descriptorSet = descriptorSets[i];
            m_bindings.push_back(binding);
        }
    } catch (const std::exception& error) {
        errorMessage = error.what();
        reset();
        return false;
    }
    return ready();
}

bool PBRMaterialResources::uploadTexture(
    TextureFactory& textureFactory,
    const asset::TextureData& data,
    VkFormat format,
    std::unique_ptr<RHITexture>& destination)
{
    if (!data.valid()) {
        return false;
    }
    try {
        destination = textureFactory.createFromRgba8(data, format);
        return destination != nullptr;
    } catch (const std::exception& error) {
        KU_WARN("PBR material texture upload failed: {}", error.what());
        destination.reset();
        return false;
    }
}

bool PBRMaterialResources::ready() const
{
    return m_device != VK_NULL_HANDLE
        && m_descriptorSetLayout != VK_NULL_HANDLE
        && m_descriptorPool != VK_NULL_HANDLE
        && m_sampler != VK_NULL_HANDLE
        && !m_bindings.empty();
}

const PBRMaterialBinding& PBRMaterialResources::material(
    uint32_t index) const
{
    if (m_bindings.empty()) {
        throw std::runtime_error("PBR material resources are not initialized");
    }
    return m_bindings[index < m_bindings.size() ? index : 0u];
}

void PBRMaterialResources::reset()
{
    m_bindings.clear();
    destroyVulkanHandles();
    m_materialTextures.clear();
    m_fallbackWhite.reset();
    m_fallbackNormal.reset();
    m_fallbackOrm.reset();
    m_fallbackEmissive.reset();
    m_stats = {};
}

void PBRMaterialResources::destroyVulkanHandles()
{
    if (m_device == VK_NULL_HANDLE) {
        return;
    }
    if (m_descriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(m_device, m_descriptorPool, nullptr);
        m_descriptorPool = VK_NULL_HANDLE;
    }
    if (m_sampler != VK_NULL_HANDLE) {
        vkDestroySampler(m_device, m_sampler, nullptr);
        m_sampler = VK_NULL_HANDLE;
    }
    if (m_descriptorSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(
            m_device,
            m_descriptorSetLayout,
            nullptr);
        m_descriptorSetLayout = VK_NULL_HANDLE;
    }
    m_device = VK_NULL_HANDLE;
}

PBREnvironmentResources::PBREnvironmentResources() = default;

PBREnvironmentResources::~PBREnvironmentResources()
{
    reset();
}

bool PBREnvironmentResources::initialize(
    RHIDevice& device,
    TextureFactory& textureFactory,
    const asset::HDRImageData& image,
    std::string& errorMessage)
{
    reset();
    errorMessage.clear();
    m_device = device.device();
    try {
        if (!image.valid()) {
            throw std::invalid_argument("PBR environment requires valid HDR pixels");
        }
        if (image.rgba32f.size()
            > std::numeric_limits<VkDeviceSize>::max() / sizeof(float)) {
            throw std::overflow_error("PBR environment upload size overflows VkDeviceSize");
        }
        const VkDeviceSize byteSize =
            static_cast<VkDeviceSize>(image.rgba32f.size() * sizeof(float));
        m_texture = textureFactory.createTexture2D(
            image.rgba32f.data(),
            byteSize,
            image.width,
            image.height,
            VK_FORMAT_R32G32B32A32_SFLOAT);

        VkDescriptorSetLayoutBinding binding{};
        binding.binding = 0;
        binding.descriptorType =
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        binding.descriptorCount = 1;
        binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType =
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = 1;
        layoutInfo.pBindings = &binding;
        VK_CHECK(vkCreateDescriptorSetLayout(
            m_device,
            &layoutInfo,
            nullptr,
            &m_descriptorSetLayout));

        VkDescriptorPoolSize poolSize{};
        poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        poolSize.descriptorCount = 1;
        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.maxSets = 1;
        poolInfo.poolSizeCount = 1;
        poolInfo.pPoolSizes = &poolSize;
        VK_CHECK(vkCreateDescriptorPool(
            m_device,
            &poolInfo,
            nullptr,
            &m_descriptorPool));

        VkSamplerCreateInfo samplerInfo{};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter = VK_FILTER_LINEAR;
        samplerInfo.minFilter = VK_FILTER_LINEAR;
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.minLod = 0.0f;
        samplerInfo.maxLod = 0.0f;
        samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
        VK_CHECK(vkCreateSampler(
            m_device,
            &samplerInfo,
            nullptr,
            &m_sampler));

        VkDescriptorSetAllocateInfo allocateInfo{};
        allocateInfo.sType =
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocateInfo.descriptorPool = m_descriptorPool;
        allocateInfo.descriptorSetCount = 1;
        allocateInfo.pSetLayouts = &m_descriptorSetLayout;
        VK_CHECK(vkAllocateDescriptorSets(
            m_device,
            &allocateInfo,
            &m_descriptorSet));

        VkDescriptorImageInfo imageInfo{};
        imageInfo.sampler = m_sampler;
        imageInfo.imageView = m_texture->imageView();
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        VkWriteDescriptorSet write{};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = m_descriptorSet;
        write.dstBinding = 0;
        write.descriptorType =
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        write.descriptorCount = 1;
        write.pImageInfo = &imageInfo;
        vkUpdateDescriptorSets(m_device, 1, &write, 0, nullptr);
    } catch (const std::exception& error) {
        errorMessage = error.what();
        reset();
        return false;
    }
    return ready();
}

bool PBREnvironmentResources::ready() const
{
    return m_device != VK_NULL_HANDLE
        && m_texture != nullptr
        && m_descriptorSetLayout != VK_NULL_HANDLE
        && m_descriptorPool != VK_NULL_HANDLE
        && m_descriptorSet != VK_NULL_HANDLE
        && m_sampler != VK_NULL_HANDLE;
}

void PBREnvironmentResources::reset()
{
    destroyVulkanHandles();
    m_texture.reset();
}

void PBREnvironmentResources::destroyVulkanHandles()
{
    if (m_device == VK_NULL_HANDLE) {
        return;
    }
    if (m_descriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(m_device, m_descriptorPool, nullptr);
        m_descriptorPool = VK_NULL_HANDLE;
    }
    if (m_sampler != VK_NULL_HANDLE) {
        vkDestroySampler(m_device, m_sampler, nullptr);
        m_sampler = VK_NULL_HANDLE;
    }
    if (m_descriptorSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(
            m_device,
            m_descriptorSetLayout,
            nullptr);
        m_descriptorSetLayout = VK_NULL_HANDLE;
    }
    m_descriptorSet = VK_NULL_HANDLE;
    m_device = VK_NULL_HANDLE;
}

} // namespace ku
