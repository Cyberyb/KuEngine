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
    if (sourceCount
        > std::numeric_limits<uint32_t>::max()
            / pbr_material_binding::count) {
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

PBROptionalTexturePayload classifyOptionalTexturePayload(
    const asset::TextureData& data) noexcept
{
    if (data.valid()) {
        return PBROptionalTexturePayload::Ready;
    }
    return data.width == 0 && data.height == 0 && data.rgba8.empty()
        ? PBROptionalTexturePayload::Missing
        : PBROptionalTexturePayload::Invalid;
}

bool hasTextureSourceOverride(
    const asset::MaterialConfig::TextureBindingConfig& binding) noexcept
{
    return binding.hasSource;
}

PBRMaterialTextureSourcePlan buildPBRMaterialTextureSourcePlan(
    const asset::MaterialData& material,
    const asset::MaterialConfig* config)
{
    PBRMaterialTextureSourcePlan plan{};
    plan.baseColor = &material.baseColorTexture;
    plan.normal = &material.normalTexture;
    plan.metallicRoughness = &material.metallicRoughnessTexture;
    plan.occlusion = &material.occlusionTexture;
    plan.emissive = &material.emissiveTexture;
    if (config == nullptr) {
        return plan;
    }

    const auto resolve =
        [&](const asset::MaterialConfig::TextureBindingConfig* binding,
            const asset::TextureData* embedded,
            MaterialTextureSemantic semantic,
            bool legacyCombined,
            uint32_t bindingIndex) {
            if (binding == nullptr || !hasTextureSourceOverride(*binding)) {
                return embedded;
            }
            const std::string source = toLower(binding->source);
            if (isDisabledSource(source)) {
                return static_cast<const asset::TextureData*>(nullptr);
            }
            const asset::TextureData* resolved =
                resolveGltfTexture(binding->source, material, semantic);
            if (legacyCombined
                && (resolved == nullptr
                    || classifyOptionalTexturePayload(*resolved)
                        != PBROptionalTexturePayload::Ready)) {
                const asset::TextureData* combined =
                    resolveGltfTexture(binding->source, material);
                if (combined != nullptr && combined->valid()) {
                    resolved = combined;
                }
            }
            if (resolved == nullptr) {
                plan.unsupportedOverrides[bindingIndex] = true;
            }
            return resolved;
        };

    plan.baseColor = resolve(
        &config->baseColorBinding,
        plan.baseColor,
        MaterialTextureSemantic::BaseColor,
        false,
        pbr_material_binding::baseColor);
    plan.normal = resolve(
        &config->normalBinding,
        plan.normal,
        MaterialTextureSemantic::Normal,
        false,
        pbr_material_binding::normal);

    const bool hasCombinedOrm = isCombinedOrmBinding(config->ormBinding);
    const auto sourceBinding =
        [&](const asset::MaterialConfig::TextureBindingConfig& preferred) {
            if (hasTextureSourceOverride(preferred)) {
                return &preferred;
            }
            if (hasCombinedOrm && hasTextureSourceOverride(config->ormBinding)) {
                return &config->ormBinding;
            }
            return static_cast<
                const asset::MaterialConfig::TextureBindingConfig*>(nullptr);
        };
    const auto* mrBinding = sourceBinding(config->metallicRoughnessBinding);
    const auto* aoBinding = sourceBinding(config->occlusionBinding);
    plan.metallicRoughness = resolve(
        mrBinding,
        plan.metallicRoughness,
        MaterialTextureSemantic::MetallicRoughness,
        mrBinding == &config->ormBinding,
        pbr_material_binding::metallicRoughness);
    plan.occlusion = resolve(
        aoBinding,
        plan.occlusion,
        MaterialTextureSemantic::Occlusion,
        aoBinding == &config->ormBinding,
        pbr_material_binding::occlusion);
    plan.emissive = resolve(
        &config->emissiveBinding,
        plan.emissive,
        MaterialTextureSemantic::Emissive,
        false,
        pbr_material_binding::emissive);
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
    std::string& errorMessage,
    VkDescriptorSetLayout externalLayout)
{
    reset();
    errorMessage.clear();
    m_device = device.device();

    try {
        const PBRMaterialBuildPlan plan = buildPBRMaterialPlan(mesh);

        if (externalLayout != VK_NULL_HANDLE) {
            m_descriptorSetLayout = externalLayout;
            m_ownsDescriptorSetLayout = false;
        } else {
            std::array<VkDescriptorSetLayoutBinding,
                pbr_material_binding::count> layoutBindings{};
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
                m_device, &layoutInfo, nullptr, &m_descriptorSetLayout));
            m_ownsDescriptorSetLayout = true;
        }

        VkDescriptorPoolSize poolSize{};
        poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        poolSize.descriptorCount =
            plan.materialCount * pbr_material_binding::count;
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
        m_fallbackMetallicRoughness = textureFactory.createSolidColor(
            {255, 255, 0, 255},
            VK_FORMAT_R8G8B8A8_UNORM);
        m_fallbackOcclusion = textureFactory.createSolidColor(
            {255, 255, 255, 255},
            VK_FORMAT_R8G8B8A8_UNORM);
        m_fallbackEmissive = textureFactory.createSolidColor(
            {0, 0, 0, 255},
            VK_FORMAT_R8G8B8A8_UNORM);
        if (!m_fallbackWhite || !m_fallbackNormal
            || !m_fallbackMetallicRoughness || !m_fallbackOcclusion
            || !m_fallbackEmissive) {
            throw std::runtime_error(
                "PBR fallback texture creation returned no texture");
        }

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
            static_cast<size_t>(plan.materialCount)
                * pbr_material_binding::count);
        const asset::MaterialData fallbackMaterial{};

        for (uint32_t i = 0; i < plan.materialCount; ++i) {
            const asset::MaterialData& material = i < mesh.materials.size()
                ? mesh.materials[i]
                : fallbackMaterial;

            const PBRMaterialTextureSourcePlan textureSourcePlan =
                buildPBRMaterialTextureSourcePlan(material, config);
            const asset::TextureData* baseSource = textureSourcePlan.baseColor;
            const asset::TextureData* normalSource = textureSourcePlan.normal;
            const asset::TextureData* metallicRoughnessSource =
                textureSourcePlan.metallicRoughness;
            const asset::TextureData* occlusionSource =
                textureSourcePlan.occlusion;
            const asset::TextureData* emissiveSource =
                textureSourcePlan.emissive;
            const asset::MaterialConfig::TextureBindingConfig emptyBinding{};
            constexpr std::array<const char*, pbr_material_binding::count>
                bindingNames{
                    "baseColor",
                    "normal",
                    "metallicRoughness",
                    "occlusion",
                    "emissive",
                };
            for (uint32_t bindingIndex = 0;
                 bindingIndex < bindingNames.size();
                 ++bindingIndex) {
                if (textureSourcePlan.unsupportedOverrides[bindingIndex]) {
                    KU_WARN(
                        "PBRMaterialResources: {} source override is unsupported",
                        bindingNames[bindingIndex]);
                    ++m_stats.optionalTextureFallbacks;
                }
            }

            std::unique_ptr<RHITexture> baseTexture;
            std::unique_ptr<RHITexture> normalTexture;
            std::unique_ptr<RHITexture> metallicRoughnessTexture;
            std::unique_ptr<RHITexture> occlusionTexture;
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
                    "baseColor",
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
                    "normal",
                    normalTexture);
            const auto configuredBinding = [&](const auto& preferred) -> const auto& {
                if (config == nullptr) {
                    return emptyBinding;
                }
                if (hasTextureSourceOverride(preferred)
                    || preferred.hasColorSpace) {
                    return preferred;
                }
                return config->ormBinding;
            };
            const bool hasMetallicRoughness =
                metallicRoughnessSource != nullptr
                && uploadTexture(
                    textureFactory,
                    *metallicRoughnessSource,
                    formatForBinding(
                        configuredBinding(
                            config != nullptr
                                ? config->metallicRoughnessBinding
                                : emptyBinding),
                        VK_FORMAT_R8G8B8A8_UNORM,
                        "metallicRoughness"),
                    "metallicRoughness",
                    metallicRoughnessTexture);
            const bool hasOcclusion = occlusionSource != nullptr
                && uploadTexture(
                    textureFactory,
                    *occlusionSource,
                    formatForBinding(
                        configuredBinding(
                            config != nullptr
                                ? config->occlusionBinding
                                : emptyBinding),
                        VK_FORMAT_R8G8B8A8_UNORM,
                        "occlusion"),
                    "occlusion",
                    occlusionTexture);
            const bool hasEmissive = emissiveSource != nullptr
                && uploadTexture(
                    textureFactory,
                    *emissiveSource,
                    VK_FORMAT_R8G8B8A8_SRGB,
                    "emissive",
                    emissiveTexture);

            VkImageView baseView = m_fallbackWhite->imageView();
            VkImageView normalView = m_fallbackNormal->imageView();
            VkImageView metallicRoughnessView =
                m_fallbackMetallicRoughness->imageView();
            VkImageView occlusionView = m_fallbackOcclusion->imageView();
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
            retain(
                metallicRoughnessTexture,
                metallicRoughnessView,
                m_stats.texturedMetallicRoughness);
            retain(
                occlusionTexture,
                occlusionView,
                m_stats.texturedOcclusion);
            retain(
                emissiveTexture,
                emissiveView,
                m_stats.texturedEmissive);

            std::array<
                VkDescriptorImageInfo,
                pbr_material_binding::count> imageInfos{
                VkDescriptorImageInfo{m_sampler, baseView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
                VkDescriptorImageInfo{m_sampler, normalView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
                VkDescriptorImageInfo{m_sampler, metallicRoughnessView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
                VkDescriptorImageInfo{m_sampler, occlusionView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
                VkDescriptorImageInfo{m_sampler, emissiveView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
            };
            std::array<
                VkWriteDescriptorSet,
                pbr_material_binding::count> writes{};
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
            binding.metallicRoughnessUvScaleOffset = {
                material.metallicRoughnessTransform.scale.x,
                material.metallicRoughnessTransform.scale.y,
                material.metallicRoughnessTransform.offset.x,
                material.metallicRoughnessTransform.offset.y,
            };
            binding.occlusionUvScaleOffset = {
                material.occlusionTransform.scale.x,
                material.occlusionTransform.scale.y,
                material.occlusionTransform.offset.x,
                material.occlusionTransform.offset.y,
            };
            binding.emissiveUvScaleOffset = {
                material.emissiveTransform.scale.x,
                material.emissiveTransform.scale.y,
                material.emissiveTransform.offset.x,
                material.emissiveTransform.offset.y,
            };
            binding.baseUvRotation = material.baseColorTransform.rotation;
            binding.normalUvRotation = material.normalTransform.rotation;
            binding.metallicRoughnessUvRotation =
                material.metallicRoughnessTransform.rotation;
            binding.occlusionUvRotation = material.occlusionTransform.rotation;
            binding.emissiveUvRotation =
                material.emissiveTransform.rotation;
            binding.baseTexCoord =
                clampTexCoordSet(material.baseColorTransform.texCoord);
            binding.normalTexCoord =
                clampTexCoordSet(material.normalTransform.texCoord);
            binding.metallicRoughnessTexCoord = clampTexCoordSet(
                material.metallicRoughnessTransform.texCoord);
            binding.occlusionTexCoord =
                clampTexCoordSet(material.occlusionTransform.texCoord);
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
            if (config != nullptr) {
                const bool useLegacyCombinedUv =
                    isCombinedOrmBinding(config->ormBinding);
                const auto applyUvSet = [&](const auto& preferred,
                                             float& destination) {
                    if (preferred.hasUvSet) {
                        destination = clampTexCoordSet(
                            static_cast<uint32_t>(preferred.uvSet));
                    } else if (useLegacyCombinedUv
                        && config->ormBinding.hasUvSet) {
                        destination = clampTexCoordSet(
                            static_cast<uint32_t>(config->ormBinding.uvSet));
                    }
                };
                applyUvSet(
                    config->metallicRoughnessBinding,
                    binding.metallicRoughnessTexCoord);
                applyUvSet(
                    config->occlusionBinding,
                    binding.occlusionTexCoord);
                if (config->emissiveBinding.hasUvSet) {
                    binding.emissiveTexCoord = clampTexCoordSet(
                        static_cast<uint32_t>(config->emissiveBinding.uvSet));
                }
            }
            binding.hasBaseColorTexture = hasBase;
            binding.hasNormalTexture = hasNormal;
            binding.hasMetallicRoughnessTexture = hasMetallicRoughness;
            binding.hasOcclusionTexture = hasOcclusion;
            binding.hasEmissiveTexture = hasEmissive;
            binding.descriptorSet = descriptorSets[i];
            m_bindings.push_back(binding);
        }
    } catch (const std::exception& error) {
        errorMessage = error.what();
        reset();
        return false;
    } catch (...) {
        errorMessage = "Unknown PBR material initialization failure";
        reset();
        return false;
    }
    if (!ready()) {
        errorMessage = "PBR material resources did not reach the ready state";
        reset();
        return false;
    }
    return true;
}

bool PBRMaterialResources::uploadTexture(
    TextureFactory& textureFactory,
    const asset::TextureData& data,
    VkFormat format,
    std::string_view bindingName,
    std::unique_ptr<RHITexture>& destination)
{
    const PBROptionalTexturePayload payload =
        classifyOptionalTexturePayload(data);
    if (payload != PBROptionalTexturePayload::Ready) {
        if (payload == PBROptionalTexturePayload::Invalid) {
            ++m_stats.optionalTextureFallbacks;
            KU_WARN(
                "PBR material {} texture payload is invalid; using fallback",
                bindingName);
        }
        return false;
    }
    destination = textureFactory.createFromRgba8(data, format);
    if (!destination) {
        throw std::runtime_error(
            "PBR material texture upload returned no texture");
    }
    return true;
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
    m_fallbackMetallicRoughness.reset();
    m_fallbackOcclusion.reset();
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
    if (m_descriptorSetLayout != VK_NULL_HANDLE
        && m_ownsDescriptorSetLayout) {
        vkDestroyDescriptorSetLayout(
            m_device,
            m_descriptorSetLayout,
            nullptr);
        m_descriptorSetLayout = VK_NULL_HANDLE;
    }
    m_descriptorSetLayout = VK_NULL_HANDLE;
    m_ownsDescriptorSetLayout = false;
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
    std::string& errorMessage,
    VkDescriptorSetLayout externalLayout)
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

        if (externalLayout != VK_NULL_HANDLE) {
            m_descriptorSetLayout = externalLayout;
            m_ownsDescriptorSetLayout = false;
        } else {
            VkDescriptorSetLayoutBinding binding{};
            binding.binding = pbr_environment_binding::environment;
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
                m_device, &layoutInfo, nullptr, &m_descriptorSetLayout));
            m_ownsDescriptorSetLayout = true;
        }

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
        write.dstBinding = pbr_environment_binding::environment;
        write.descriptorType =
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        write.descriptorCount = 1;
        write.pImageInfo = &imageInfo;
        vkUpdateDescriptorSets(m_device, 1, &write, 0, nullptr);
    } catch (const std::exception& error) {
        errorMessage = error.what();
        reset();
        return false;
    } catch (...) {
        errorMessage = "Unknown PBR environment initialization failure";
        reset();
        return false;
    }
    if (!ready()) {
        errorMessage = "PBR environment resources did not reach the ready state";
        reset();
        return false;
    }
    return true;
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
    if (m_descriptorSetLayout != VK_NULL_HANDLE
        && m_ownsDescriptorSetLayout) {
        vkDestroyDescriptorSetLayout(
            m_device,
            m_descriptorSetLayout,
            nullptr);
        m_descriptorSetLayout = VK_NULL_HANDLE;
    }
    m_descriptorSetLayout = VK_NULL_HANDLE;
    m_ownsDescriptorSetLayout = false;
    m_descriptorSet = VK_NULL_HANDLE;
    m_device = VK_NULL_HANDLE;
}

} // namespace ku
