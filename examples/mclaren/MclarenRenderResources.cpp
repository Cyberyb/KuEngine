#include "MclarenRenderResources.h"

#include "MclarenSceneAsset.h"

#include <KuEngine/Core/Log.h>
#include <KuEngine/RHI/CommandList.h>
#include <KuEngine/RHI/RHIBuffer.h>
#include <KuEngine/RHI/RHIDevice.h>
#include <KuEngine/RHI/RHIPipeline.h>
#include <KuEngine/RHI/RHIShader.h>
#include <KuEngine/RHI/RHITexture.h>
#include <KuEngine/RHI/ResourceUploader.h>
#include <KuEngine/Render/GpuMesh.h>
#include <KuEngine/Render/PBRRenderer.h>
#include <KuEngine/Render/TextureFactory.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstring>
#include <filesystem>
#include <limits>
#include <stdexcept>
#include <vector>

#include <stb_image.h>

namespace ku {

namespace {

} // namespace

MclarenRenderResources::MclarenRenderResources() = default;

MclarenRenderResources::~MclarenRenderResources()
{
    reset();
}

bool MclarenRenderResources::initialize(
    RHIDevice& device,
    const MclarenSceneAsset& scene,
    VkFormat colorFormat,
    VkFormat depthFormat,
    VkCompareOp depthCompareOp,
    std::string& errorMessage)
{
    reset();
    errorMessage.clear();
    m_deviceHandle = device.device();

    try {
        m_resourceUploader = std::make_unique<ResourceUploader>(device);
        m_textureFactory =
            std::make_unique<TextureFactory>(device, *m_resourceUploader);

        const std::filesystem::path shaderDirectory =
            std::filesystem::current_path() / "shaders";
        m_vertShader = std::make_unique<RHIShader>(
            device,
            shaderDirectory / "mclaren.vert.spv");
        m_fragShader = std::make_unique<RHIShader>(
            device,
            shaderDirectory / "mclaren.frag.spv");
        m_skyboxVertShader = std::make_unique<RHIShader>(
            device,
            shaderDirectory / "skybox.vert.spv");
        m_skyboxFragShader = std::make_unique<RHIShader>(
            device,
            shaderDirectory / "skybox.frag.spv");

        m_gpuMesh = std::make_unique<GpuMesh>(
            device,
            *m_resourceUploader,
            scene.mesh());

        createDescriptorLayouts(device);
        createFrameResources(device, m_gpuMesh->subMeshes().size());
        createMaterialResources(scene);
        createEnvironmentResources(scene);
        createPipelines(
            device,
            scene,
            colorFormat,
            depthFormat,
            depthCompareOp);
        configurePbrRenderer(depthFormat);
    } catch (const std::exception& error) {
        errorMessage = error.what();
        KU_ERROR(
            "MclarenRenderResources: initialization failed: {}",
            errorMessage);
        reset();
        return false;
    }

    KU_INFO(
        "MclarenRenderResources initialized: vertices={}, indices={}, materials={}, subMeshes={}, textured(base/normal/orm/emissive)=({}/{}/{}/{})",
        m_gpuMesh->vertexCount(),
        m_gpuMesh->indexCount(),
        m_materialBindings.size(),
        m_gpuMesh->subMeshes().size(),
        m_stats.texturedBase,
        m_stats.texturedNormal,
        m_stats.texturedOrm,
        m_stats.texturedEmissive);
    return true;
}

void MclarenRenderResources::reset()
{
    m_pbrRenderer.reset();
    m_pipeline.reset();
    m_skyboxPipeline.reset();
    m_vertShader.reset();
    m_fragShader.reset();
    m_skyboxVertShader.reset();
    m_skyboxFragShader.reset();
    m_frameUniformBuffer.reset();
    m_frameUniformStride = 0;
    m_materialBindings.clear();
    m_materialTextures.clear();
    m_gpuMesh.reset();
    m_environmentTexture.reset();
    m_fallbackWhiteTexture.reset();
    m_fallbackNormalTexture.reset();
    m_fallbackOrmTexture.reset();
    m_fallbackEmissiveTexture.reset();
    m_textureFactory.reset();
    m_resourceUploader.reset();

    destroyVulkanHandles();
    m_stats = {};
}

bool MclarenRenderResources::ready() const
{
    return m_pipeline
        && m_gpuMesh
        && m_pbrRenderer
        && !m_materialBindings.empty();
}

GpuMesh& MclarenRenderResources::mesh() const
{
    if (!m_gpuMesh) {
        throw std::runtime_error("Mclaren GPU mesh is not initialized");
    }
    return *m_gpuMesh;
}

const std::vector<PBRMaterialBinding>&
MclarenRenderResources::materials() const
{
    return m_materialBindings;
}

PBRRenderer& MclarenRenderResources::pbrRenderer() const
{
    if (!m_pbrRenderer) {
        throw std::runtime_error("Mclaren PBR renderer is not initialized");
    }
    return *m_pbrRenderer;
}

void MclarenRenderResources::createDescriptorLayouts(RHIDevice& device)
{
    std::array<VkDescriptorSetLayoutBinding, 4> textureBindings{};
    for (uint32_t i = 0; i < textureBindings.size(); ++i) {
        textureBindings[i].binding = i;
        textureBindings[i].descriptorType =
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        textureBindings[i].descriptorCount = 1;
        textureBindings[i].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    }

    VkDescriptorSetLayoutCreateInfo materialLayoutInfo{};
    materialLayoutInfo.sType =
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    materialLayoutInfo.bindingCount =
        static_cast<uint32_t>(textureBindings.size());
    materialLayoutInfo.pBindings = textureBindings.data();
    VK_CHECK(vkCreateDescriptorSetLayout(
        m_deviceHandle,
        &materialLayoutInfo,
        nullptr,
        &m_materialDescriptorSetLayout));

    VkDescriptorSetLayoutBinding frameBinding{};
    frameBinding.binding = 0;
    frameBinding.descriptorType =
        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
    frameBinding.descriptorCount = 1;
    frameBinding.stageFlags =
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo frameLayoutInfo{};
    frameLayoutInfo.sType =
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    frameLayoutInfo.bindingCount = 1;
    frameLayoutInfo.pBindings = &frameBinding;
    VK_CHECK(vkCreateDescriptorSetLayout(
        m_deviceHandle,
        &frameLayoutInfo,
        nullptr,
        &m_frameDescriptorSetLayout));

    VkDescriptorSetLayoutBinding environmentBinding{};
    environmentBinding.binding = 0;
    environmentBinding.descriptorType =
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    environmentBinding.descriptorCount = 1;
    environmentBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo environmentLayoutInfo{};
    environmentLayoutInfo.sType =
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    environmentLayoutInfo.bindingCount = 1;
    environmentLayoutInfo.pBindings = &environmentBinding;
    VK_CHECK(vkCreateDescriptorSetLayout(
        m_deviceHandle,
        &environmentLayoutInfo,
        nullptr,
        &m_environmentDescriptorSetLayout));

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
        m_deviceHandle,
        &samplerInfo,
        nullptr,
        &m_sampler));
}

void MclarenRenderResources::createFrameResources(
    RHIDevice& device,
    size_t drawCount)
{
    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
    poolSize.descriptorCount = 1;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.maxSets = 1;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    VK_CHECK(vkCreateDescriptorPool(
        m_deviceHandle,
        &poolInfo,
        nullptr,
        &m_frameDescriptorPool));

    VkDescriptorSetAllocateInfo allocateInfo{};
    allocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocateInfo.descriptorPool = m_frameDescriptorPool;
    allocateInfo.descriptorSetCount = 1;
    allocateInfo.pSetLayouts = &m_frameDescriptorSetLayout;
    VK_CHECK(vkAllocateDescriptorSets(
        m_deviceHandle,
        &allocateInfo,
        &m_frameDescriptorSet));

    const VkDeviceSize stride = alignedUniformBufferStride(
        sizeof(PBRFrameUniforms),
        device.properties().limits.minUniformBufferOffsetAlignment);
    if (stride > std::numeric_limits<uint32_t>::max()) {
        throw std::runtime_error(
            "PBR frame uniform stride exceeds Vulkan dynamic offset range");
    }
    m_frameUniformStride = static_cast<uint32_t>(stride);

    const size_t actualDrawCount = std::max<size_t>(1, drawCount);
    if (actualDrawCount
        > std::numeric_limits<uint32_t>::max() / m_frameUniformStride) {
        throw std::runtime_error(
            "PBR frame uniform buffer exceeds Vulkan dynamic offset range");
    }

    RHIBuffer::CreateInfo bufferInfo{};
    bufferInfo.size =
        static_cast<VkDeviceSize>(m_frameUniformStride)
        * static_cast<VkDeviceSize>(actualDrawCount + 1);
    bufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    bufferInfo.memoryUsage = VMA_MEMORY_USAGE_AUTO;
    bufferInfo.allocationFlags =
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT
        | VMA_ALLOCATION_CREATE_MAPPED_BIT;
    m_frameUniformBuffer =
        std::make_unique<RHIBuffer>(device, bufferInfo);

    VkDescriptorBufferInfo descriptorBuffer{};
    descriptorBuffer.buffer = m_frameUniformBuffer->buffer();
    descriptorBuffer.offset = 0;
    descriptorBuffer.range = sizeof(PBRFrameUniforms);

    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = m_frameDescriptorSet;
    write.dstBinding = 0;
    write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
    write.descriptorCount = 1;
    write.pBufferInfo = &descriptorBuffer;
    vkUpdateDescriptorSets(
        m_deviceHandle,
        1,
        &write,
        0,
        nullptr);
}

bool MclarenRenderResources::createAndUploadTexture(
    const asset::TextureData& textureData,
    VkFormat format,
    std::unique_ptr<RHITexture>& outTexture)
{
    if (!textureData.valid() || !m_textureFactory) {
        return false;
    }

    try {
        outTexture =
            m_textureFactory->createFromRgba8(textureData, format);
        return true;
    } catch (const std::exception& error) {
        KU_WARN(
            "MclarenRenderResources: texture upload failed: {}",
            error.what());
        outTexture.reset();
        return false;
    }
}

bool MclarenRenderResources::createSolidColorTexture(
    std::array<uint8_t, 4> rgba,
    VkFormat format,
    std::unique_ptr<RHITexture>& outTexture)
{
    if (!m_textureFactory) {
        return false;
    }

    try {
        outTexture = m_textureFactory->createSolidColor(rgba, format);
        return true;
    } catch (const std::exception& error) {
        KU_WARN(
            "MclarenRenderResources: fallback texture creation failed: {}",
            error.what());
        outTexture.reset();
        return false;
    }
}

bool MclarenRenderResources::createAndUploadHdrTexture(
    const std::filesystem::path& hdrPath,
    std::unique_ptr<RHITexture>& outTexture)
{
    int width = 0;
    int height = 0;
    int channels = 0;

    stbi_set_flip_vertically_on_load(false);
    float* pixels = stbi_loadf(
        hdrPath.string().c_str(),
        &width,
        &height,
        &channels,
        4);
    if (!pixels || width <= 0 || height <= 0 || !m_textureFactory) {
        if (pixels) {
            stbi_image_free(pixels);
        }
        return false;
    }

    const VkDeviceSize byteSize =
        static_cast<VkDeviceSize>(
            static_cast<uint64_t>(width)
            * static_cast<uint64_t>(height)
            * 4ull
            * sizeof(float));

    try {
        outTexture = m_textureFactory->createTexture2D(
            pixels,
            byteSize,
            static_cast<uint32_t>(width),
            static_cast<uint32_t>(height),
            VK_FORMAT_R32G32B32A32_SFLOAT);
    } catch (const std::exception& error) {
        KU_WARN(
            "MclarenRenderResources: HDR texture upload failed: {}",
            error.what());
        outTexture.reset();
    }

    stbi_image_free(pixels);
    return outTexture != nullptr;
}

void MclarenRenderResources::createMaterialResources(
    const MclarenSceneAsset& scene)
{
    const asset::MeshData& mesh = scene.mesh();
    const asset::MaterialConfig& config = scene.materialConfig();
    const bool useConfig = scene.materialConfigUsed();
    const uint32_t materialCount =
        std::max(1u, static_cast<uint32_t>(mesh.materials.size()));

    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSize.descriptorCount = materialCount * 4u;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.maxSets = materialCount;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    VK_CHECK(vkCreateDescriptorPool(
        m_deviceHandle,
        &poolInfo,
        nullptr,
        &m_materialDescriptorPool));

    if (!createSolidColorTexture(
            {255, 255, 255, 255},
            VK_FORMAT_R8G8B8A8_SRGB,
            m_fallbackWhiteTexture)) {
        throw std::runtime_error(
            "Fallback baseColor texture upload failed");
    }
    if (!createSolidColorTexture(
            {128, 128, 255, 255},
            VK_FORMAT_R8G8B8A8_UNORM,
            m_fallbackNormalTexture)) {
        throw std::runtime_error(
            "Fallback normal texture upload failed");
    }
    if (!createSolidColorTexture(
            {255, 255, 0, 255},
            VK_FORMAT_R8G8B8A8_UNORM,
            m_fallbackOrmTexture)) {
        throw std::runtime_error(
            "Fallback ORM texture upload failed");
    }
    if (!createSolidColorTexture(
            {0, 0, 0, 255},
            VK_FORMAT_R8G8B8A8_UNORM,
            m_fallbackEmissiveTexture)) {
        throw std::runtime_error(
            "Fallback emissive texture upload failed");
    }

    std::vector<VkDescriptorSetLayout> layouts(
        materialCount,
        m_materialDescriptorSetLayout);
    std::vector<VkDescriptorSet> descriptorSets(
        materialCount,
        VK_NULL_HANDLE);

    VkDescriptorSetAllocateInfo allocateInfo{};
    allocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocateInfo.descriptorPool = m_materialDescriptorPool;
    allocateInfo.descriptorSetCount = materialCount;
    allocateInfo.pSetLayouts = layouts.data();
    VK_CHECK(vkAllocateDescriptorSets(
        m_deviceHandle,
        &allocateInfo,
        descriptorSets.data()));

    m_materialBindings.reserve(materialCount);
    m_materialTextures.reserve(materialCount * 4u);

    for (uint32_t i = 0; i < materialCount; ++i) {
        asset::MaterialData material{};
        if (i < mesh.materials.size()) {
            material = mesh.materials[i];
        }

        const asset::TextureData* baseSource =
            &material.baseColorTexture;
        const asset::TextureData* normalSource =
            &material.normalTexture;
        const asset::TextureData* ormSource =
            &material.ormTexture;
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
                if (!resolved) {
                    KU_WARN(
                        "MclarenRenderResources: {} binding source not supported: {}",
                        bindingName,
                        binding.source);
                }
                return resolved;
            };

        baseSource = resolveConfiguredSource(
            config.baseColorBinding,
            baseSource,
            "baseColor");
        normalSource = resolveConfiguredSource(
            config.normalBinding,
            normalSource,
            "normal");
        ormSource = resolveConfiguredSource(
            config.ormBinding,
            ormSource,
            "orm");

        std::unique_ptr<RHITexture> baseTexture;
        std::unique_ptr<RHITexture> normalTexture;
        std::unique_ptr<RHITexture> ormTexture;
        std::unique_ptr<RHITexture> emissiveTexture;

        const bool hasBase =
            baseSource
            && createAndUploadTexture(
                *baseSource,
                formatForBinding(
                    config.baseColorBinding,
                    VK_FORMAT_R8G8B8A8_SRGB,
                    "baseColor"),
                baseTexture);
        const bool hasNormal =
            normalSource
            && createAndUploadTexture(
                *normalSource,
                formatForBinding(
                    config.normalBinding,
                    VK_FORMAT_R8G8B8A8_UNORM,
                    "normal"),
                normalTexture);
        const bool hasOrm =
            ormSource
            && createAndUploadTexture(
                *ormSource,
                formatForBinding(
                    config.ormBinding,
                    VK_FORMAT_R8G8B8A8_UNORM,
                    "orm"),
                ormTexture);
        const bool hasEmissive =
            emissiveSource
            && createAndUploadTexture(
                *emissiveSource,
                VK_FORMAT_R8G8B8A8_SRGB,
                emissiveTexture);

        VkImageView baseView = m_fallbackWhiteTexture->imageView();
        VkImageView normalView = m_fallbackNormalTexture->imageView();
        VkImageView ormView = m_fallbackOrmTexture->imageView();
        VkImageView emissiveView =
            m_fallbackEmissiveTexture->imageView();

        const auto retainTexture =
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
        retainTexture(baseTexture, baseView, m_stats.texturedBase);
        retainTexture(normalTexture, normalView, m_stats.texturedNormal);
        retainTexture(ormTexture, ormView, m_stats.texturedOrm);
        retainTexture(
            emissiveTexture,
            emissiveView,
            m_stats.texturedEmissive);

        std::array<VkDescriptorImageInfo, 4> imageInfos{
            VkDescriptorImageInfo{
                m_sampler,
                baseView,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
            VkDescriptorImageInfo{
                m_sampler,
                normalView,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
            VkDescriptorImageInfo{
                m_sampler,
                ormView,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
            VkDescriptorImageInfo{
                m_sampler,
                emissiveView,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
        };
        std::array<VkWriteDescriptorSet, 4> writes{};
        for (uint32_t binding = 0; binding < writes.size(); ++binding) {
            writes[binding].sType =
                VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[binding].dstSet = descriptorSets[i];
            writes[binding].dstBinding = binding;
            writes[binding].descriptorType =
                VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            writes[binding].descriptorCount = 1;
            writes[binding].pImageInfo = &imageInfos[binding];
        }
        vkUpdateDescriptorSets(
            m_deviceHandle,
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

        if (useConfig && config.baseColorBinding.hasUvSet) {
            binding.baseTexCoord = clampTexCoordSet(
                static_cast<uint32_t>(config.baseColorBinding.uvSet));
        }
        if (useConfig && config.normalBinding.hasUvSet) {
            binding.normalTexCoord = clampTexCoordSet(
                static_cast<uint32_t>(config.normalBinding.uvSet));
        }
        if (useConfig && config.ormBinding.hasUvSet) {
            binding.ormTexCoord = clampTexCoordSet(
                static_cast<uint32_t>(config.ormBinding.uvSet));
        }

        binding.hasBaseColorTexture = hasBase;
        binding.hasNormalTexture = hasNormal;
        binding.hasOrmTexture = hasOrm;
        binding.hasEmissiveTexture = hasEmissive;
        binding.descriptorSet = descriptorSets[i];
        m_materialBindings.push_back(binding);
    }
}

void MclarenRenderResources::createEnvironmentResources(
    const MclarenSceneAsset& scene)
{
    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSize.descriptorCount = 1;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.maxSets = 1;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    VK_CHECK(vkCreateDescriptorPool(
        m_deviceHandle,
        &poolInfo,
        nullptr,
        &m_environmentDescriptorPool));

    VkDescriptorSetAllocateInfo allocateInfo{};
    allocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocateInfo.descriptorPool = m_environmentDescriptorPool;
    allocateInfo.descriptorSetCount = 1;
    allocateInfo.pSetLayouts = &m_environmentDescriptorSetLayout;
    VK_CHECK(vkAllocateDescriptorSets(
        m_deviceHandle,
        &allocateInfo,
        &m_environmentDescriptorSet));

    if (!createAndUploadHdrTexture(
            scene.environmentPath(),
            m_environmentTexture)) {
        KU_WARN(
            "MclarenRenderResources: environment HDR fallback to white texture: {}",
            scene.environmentPath().string());
    }

    const VkImageView environmentView = m_environmentTexture
        ? m_environmentTexture->imageView()
        : m_fallbackWhiteTexture->imageView();
    VkDescriptorImageInfo imageInfo{};
    imageInfo.sampler = m_sampler;
    imageInfo.imageView = environmentView;
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = m_environmentDescriptorSet;
    write.dstBinding = 0;
    write.descriptorType =
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    write.descriptorCount = 1;
    write.pImageInfo = &imageInfo;
    vkUpdateDescriptorSets(
        m_deviceHandle,
        1,
        &write,
        0,
        nullptr);
}

void MclarenRenderResources::createPipelines(
    RHIDevice& device,
    const MclarenSceneAsset& scene,
    VkFormat colorFormat,
    VkFormat depthFormat,
    VkCompareOp depthCompareOp)
{
    VkVertexInputBindingDescription vertexBinding{};
    vertexBinding.binding = 0;
    vertexBinding.stride = sizeof(asset::MeshVertex);
    vertexBinding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    std::array<VkVertexInputAttributeDescription, 5> attributes{};
    attributes[0] = VkVertexInputAttributeDescription{
        0,
        0,
        VK_FORMAT_R32G32B32_SFLOAT,
        static_cast<uint32_t>(offsetof(asset::MeshVertex, position)),
    };
    attributes[1] = VkVertexInputAttributeDescription{
        1,
        0,
        VK_FORMAT_R32G32B32_SFLOAT,
        static_cast<uint32_t>(offsetof(asset::MeshVertex, normal)),
    };
    attributes[2] = VkVertexInputAttributeDescription{
        2,
        0,
        VK_FORMAT_R32G32_SFLOAT,
        static_cast<uint32_t>(offsetof(asset::MeshVertex, uv0)),
    };
    attributes[3] = VkVertexInputAttributeDescription{
        3,
        0,
        VK_FORMAT_R32G32_SFLOAT,
        static_cast<uint32_t>(offsetof(asset::MeshVertex, uv1)),
    };
    attributes[4] = VkVertexInputAttributeDescription{
        4,
        0,
        VK_FORMAT_R32G32B32A32_SFLOAT,
        static_cast<uint32_t>(offsetof(asset::MeshVertex, tangent)),
    };

    if (sizeof(PBRPushConstants)
        > device.properties().limits.maxPushConstantsSize) {
        throw std::runtime_error(
            "PBR push constants exceed the selected device maxPushConstantsSize");
    }

    GraphicsPipelineDesc pbrDesc{};
    pbrDesc.shaders = {*m_vertShader, *m_fragShader};
    pbrDesc.colorFormats = {colorFormat};
    pbrDesc.vertexBindings = {vertexBinding};
    pbrDesc.vertexAttributes.assign(
        attributes.begin(),
        attributes.end());
    pbrDesc.descriptorSetLayouts = {
        m_materialDescriptorSetLayout,
        m_frameDescriptorSetLayout,
        m_environmentDescriptorSetLayout,
    };
    pbrDesc.pushConstantRanges = {
        VkPushConstantRange{
            VK_SHADER_STAGE_VERTEX_BIT
                | VK_SHADER_STAGE_FRAGMENT_BIT,
            0,
            sizeof(PBRPushConstants),
        },
    };
    pbrDesc.cullMode = VK_CULL_MODE_NONE;
    pbrDesc.depthTest = true;
    pbrDesc.depthWrite = true;
    pbrDesc.depthFormat = depthFormat;
    pbrDesc.depthCompareOp = depthCompareOp;
    pbrDesc.blendEnable =
        scene.materialConfigUsed()
        && alphaModeToFlag(scene.materialConfig()) >= 1.5f;
    m_pipeline = std::make_unique<RHIPipeline>(device, pbrDesc);

    GraphicsPipelineDesc skyboxDesc{};
    skyboxDesc.shaders = {*m_skyboxVertShader, *m_skyboxFragShader};
    skyboxDesc.colorFormats = {colorFormat};
    skyboxDesc.descriptorSetLayouts = {
        m_frameDescriptorSetLayout,
        m_environmentDescriptorSetLayout,
    };
    skyboxDesc.pushConstantRanges = {
        VkPushConstantRange{
            VK_SHADER_STAGE_FRAGMENT_BIT,
            0,
            sizeof(PBRSkyboxPushConstants),
        },
    };
    skyboxDesc.cullMode = VK_CULL_MODE_NONE;
    skyboxDesc.depthTest = false;
    skyboxDesc.depthWrite = false;
    skyboxDesc.depthFormat = depthFormat;
    m_skyboxPipeline =
        std::make_unique<RHIPipeline>(device, skyboxDesc);
}

void MclarenRenderResources::configurePbrRenderer(
    VkFormat depthFormat)
{
    m_pbrRenderer = std::make_unique<PBRRenderer>();
    m_pbrRenderer->setDepthFormat(depthFormat);
    m_pbrRenderer->setPipeline(m_pipeline.get());
    m_pbrRenderer->setFrameDescriptorSet(m_frameDescriptorSet);
    m_pbrRenderer->setEnvironmentDescriptorSet(
        m_environmentDescriptorSet);
    m_pbrRenderer->setVertexIndexBuffers(
        &m_gpuMesh->vertexBuffer(),
        &m_gpuMesh->indexBuffer());
    m_pbrRenderer->setFrameUniformBuffer(
        m_frameUniformBuffer.get(),
        m_frameUniformStride,
        m_frameUniformStride);
}

bool MclarenRenderResources::writeSkyboxFrame(
    const PBRFrameUniforms& frame)
{
    if (!m_frameUniformBuffer) {
        return false;
    }

    void* mapped = m_frameUniformBuffer->map();
    if (!mapped) {
        return false;
    }
    std::memcpy(mapped, &frame, sizeof(frame));
    m_frameUniformBuffer->flush();
    m_frameUniformBuffer->unmap();
    return true;
}

void MclarenRenderResources::drawSkybox(
    CommandList& cmd,
    float exposure,
    bool encodeOutputGamma) const
{
    if (!m_skyboxPipeline
        || m_frameDescriptorSet == VK_NULL_HANDLE
        || m_environmentDescriptorSet == VK_NULL_HANDLE) {
        return;
    }

    m_skyboxPipeline->bind(cmd);

    constexpr uint32_t frameOffset = 0;
    vkCmdBindDescriptorSets(
        cmd,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        m_skyboxPipeline->layout(),
        0,
        1,
        &m_frameDescriptorSet,
        1,
        &frameOffset);
    vkCmdBindDescriptorSets(
        cmd,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        m_skyboxPipeline->layout(),
        1,
        1,
        &m_environmentDescriptorSet,
        0,
        nullptr);

    PBRSkyboxPushConstants push{};
    push.params[0] = std::max(0.0f, exposure);
    push.params[1] = encodeOutputGamma ? 1.0f : 0.0f;
    vkCmdPushConstants(
        cmd,
        m_skyboxPipeline->layout(),
        VK_SHADER_STAGE_FRAGMENT_BIT,
        0,
        sizeof(push),
        &push);
    vkCmdDraw(cmd, 3, 1, 0, 0);
}

void MclarenRenderResources::destroyVulkanHandles()
{
    if (m_deviceHandle == VK_NULL_HANDLE) {
        return;
    }

    if (m_materialDescriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(
            m_deviceHandle,
            m_materialDescriptorPool,
            nullptr);
        m_materialDescriptorPool = VK_NULL_HANDLE;
    }
    if (m_frameDescriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(
            m_deviceHandle,
            m_frameDescriptorPool,
            nullptr);
        m_frameDescriptorPool = VK_NULL_HANDLE;
    }
    if (m_environmentDescriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(
            m_deviceHandle,
            m_environmentDescriptorPool,
            nullptr);
        m_environmentDescriptorPool = VK_NULL_HANDLE;
    }
    if (m_sampler != VK_NULL_HANDLE) {
        vkDestroySampler(m_deviceHandle, m_sampler, nullptr);
        m_sampler = VK_NULL_HANDLE;
    }
    if (m_materialDescriptorSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(
            m_deviceHandle,
            m_materialDescriptorSetLayout,
            nullptr);
        m_materialDescriptorSetLayout = VK_NULL_HANDLE;
    }
    if (m_frameDescriptorSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(
            m_deviceHandle,
            m_frameDescriptorSetLayout,
            nullptr);
        m_frameDescriptorSetLayout = VK_NULL_HANDLE;
    }
    if (m_environmentDescriptorSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(
            m_deviceHandle,
            m_environmentDescriptorSetLayout,
            nullptr);
        m_environmentDescriptorSetLayout = VK_NULL_HANDLE;
    }

    m_frameDescriptorSet = VK_NULL_HANDLE;
    m_environmentDescriptorSet = VK_NULL_HANDLE;
    m_deviceHandle = VK_NULL_HANDLE;
}

} // namespace ku
