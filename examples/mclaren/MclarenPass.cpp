#include "MclarenPass.h"

#include <KuEngine/Asset/AssetConfig.h>
#include <KuEngine/Core/Log.h>
#include <KuEngine/Render/RenderGraph.h>

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <imgui.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cctype>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <filesystem>
#include <sstream>
#include <vector>

#include <stb_image.h>

namespace ku {

namespace {

constexpr VkFormat kColorFormat = VK_FORMAT_B8G8R8A8_UNORM;
constexpr const char* kDefaultEnvironmentHdr = "citrus_orchard_road_puresky_4k.hdr";

void submitImmediate(CommandList& cmd, RHIDevice& device)
{
    cmd.end();

    VkCommandBuffer buffer = cmd.cmd();
    VkSubmitInfo submit{};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &buffer;

    VK_CHECK(vkQueueSubmit(device.graphicsQueue(), 1, &submit, VK_NULL_HANDLE));
    VK_CHECK(vkQueueWaitIdle(device.graphicsQueue()));
}

float clampTexCoordSet(uint32_t texCoord)
{
    return texCoord == 0 ? 0.0f : 1.0f;
}

std::string toLower(std::string_view text)
{
    std::string out;
    out.reserve(text.size());
    for (unsigned char ch : text) {
        out.push_back(static_cast<char>(std::tolower(ch)));
    }
    return out;
}

bool isDisabledSource(const std::string& sourceLower)
{
    return sourceLower == "none" || sourceLower == "disabled" || sourceLower == "off";
}

const asset::TextureData* resolveGltfTexture(
    std::string_view source,
    const asset::MaterialData& material)
{
    const std::string sourceLower = toLower(source);
    if (isDisabledSource(sourceLower)) {
        return nullptr;
    }

    const std::string prefix = "gltf:";
    if (sourceLower.rfind(prefix, 0) != 0) {
        return nullptr;
    }

    std::string_view rest = std::string_view(source).substr(prefix.size());
    size_t start = 0;
    while (start < rest.size()) {
        const size_t sep = rest.find('|', start);
        const size_t end = (sep == std::string_view::npos) ? rest.size() : sep;
        std::string token = toLower(rest.substr(start, end - start));

        if (token == "basecolortexture") {
            return &material.baseColorTexture;
        }
        if (token == "normaltexture") {
            return &material.normalTexture;
        }
        if (token == "metallicroughnesstexture" || token == "occlusiontexture" || token == "ormtexture") {
            return &material.ormTexture;
        }

        if (sep == std::string_view::npos) {
            break;
        }
        start = sep + 1;
    }

    return nullptr;
}

VkFormat formatForBinding(
    const asset::MaterialConfig::TextureBindingConfig& binding,
    VkFormat defaultFormat,
    std::string_view bindingName)
{
    if (!binding.hasColorSpace) {
        return defaultFormat;
    }

    const std::string colorSpace = toLower(binding.colorSpace);
    if (colorSpace == "srgb") {
        if (bindingName == "normal" || bindingName == "orm") {
            KU_WARN("MclarenPass: {} binding uses sRGB, forcing Linear", std::string(bindingName));
            return VK_FORMAT_R8G8B8A8_UNORM;
        }
        return VK_FORMAT_R8G8B8A8_SRGB;
    }
    if (colorSpace == "linear") {
        if (bindingName == "baseColor") {
            KU_WARN("MclarenPass: baseColor binding uses Linear, forcing sRGB");
            return VK_FORMAT_R8G8B8A8_SRGB;
        }
        return VK_FORMAT_R8G8B8A8_UNORM;
    }

    KU_WARN(
        "MclarenPass: {} binding uses unknown colorSpace '{}', using default format",
        std::string(bindingName),
        binding.colorSpace);
    return defaultFormat;
}

float alphaModeToFlag(const asset::MaterialConfig& config)
{
    const std::string alphaMode = toLower(config.alphaMode);
    if (alphaMode == "mask") {
        return 1.0f;
    }
    if (alphaMode == "blend") {
        return 2.0f;
    }
    return 0.0f;
}

void appendMeshData(
    const asset::MeshData& source,
    asset::MeshData& target)
{
    const uint32_t baseVertex = static_cast<uint32_t>(target.vertices.size());
    const uint32_t baseIndex = static_cast<uint32_t>(target.indices.size());
    const uint32_t baseMaterial = static_cast<uint32_t>(target.materials.size());

    target.vertices.insert(target.vertices.end(), source.vertices.begin(), source.vertices.end());
    target.indices.reserve(target.indices.size() + source.indices.size());
    for (uint32_t index : source.indices) {
        target.indices.push_back(baseVertex + index);
    }

    for (const asset::SubMeshData& subMesh : source.subMeshes) {
        target.subMeshes.push_back(asset::SubMeshData{
            baseIndex + subMesh.indexStart,
            subMesh.indexCount,
            baseMaterial + subMesh.materialIndex,
        });
    }

    target.materials.insert(target.materials.end(), source.materials.begin(), source.materials.end());

    if (baseVertex == 0) {
        target.boundsMin = source.boundsMin;
        target.boundsMax = source.boundsMax;
    } else {
        target.boundsMin = glm::min(target.boundsMin, source.boundsMin);
        target.boundsMax = glm::max(target.boundsMax, source.boundsMax);
    }
}

} // namespace

MclarenPass::MclarenPass() = default;
MclarenPass::~MclarenPass()
{
    destroyDescriptorResources();
}

void MclarenPass::setup(RenderGraphBuilder& builder)
{
    const ResourceHandle swapChainColor = builder.importExternal("SwapChainColor");
    builder.write(swapChainColor);
}

std::filesystem::path MclarenPass::resolveModelPath() const
{
    const std::filesystem::path runtimePath =
        std::filesystem::current_path() / "resources" / "models" / "props" / "mclaren_765lt.glb";

    if (std::filesystem::exists(runtimePath)) {
        return runtimePath;
    }

#ifdef KUENGINE_SOURCE_DIR
    const std::filesystem::path sourcePath =
        std::filesystem::path(KUENGINE_SOURCE_DIR) / "resources" / "models" / "props" / "mclaren_765lt.glb";
    if (std::filesystem::exists(sourcePath)) {
        return sourcePath;
    }
#endif

    return runtimePath;
}

std::filesystem::path MclarenPass::resolveScenePath() const
{
    const std::filesystem::path runtimePath =
        std::filesystem::current_path() / "resources" / "scenes" / "sandbox" / "mclaren-sandbox.scene.json";

    if (std::filesystem::exists(runtimePath)) {
        return runtimePath;
    }

#ifdef KUENGINE_SOURCE_DIR
    const std::filesystem::path sourcePath =
        std::filesystem::path(KUENGINE_SOURCE_DIR) / "resources" / "scenes" / "sandbox" / "mclaren-sandbox.scene.json";
    if (std::filesystem::exists(sourcePath)) {
        return sourcePath;
    }
#endif

    return runtimePath;
}

std::filesystem::path MclarenPass::resolveEnvironmentPath() const
{
    const std::filesystem::path runtimePath =
        std::filesystem::current_path() / "resources" / "environments" / "hdr" / kDefaultEnvironmentHdr;

    if (std::filesystem::exists(runtimePath)) {
        return runtimePath;
    }

#ifdef KUENGINE_SOURCE_DIR
    const std::filesystem::path sourcePath =
        std::filesystem::path(KUENGINE_SOURCE_DIR) / "resources" / "environments" / "hdr" / kDefaultEnvironmentHdr;
    if (std::filesystem::exists(sourcePath)) {
        return sourcePath;
    }
#endif

    return runtimePath;
}

void MclarenPass::loadMaterialConfig(const std::filesystem::path& materialPath)
{
    asset::MaterialConfig materialConfig{};
    std::string error;
    if (!asset::loadMaterialConfigFromFile(materialPath, materialConfig, &error)) {
        KU_WARN("MclarenPass: material config fallback to glTF defaults: {}", error);
        return;
    }

    m_materialConfig = materialConfig;

    if (materialConfig.hasBaseColorFactor) {
        m_globalBaseColorFactor = materialConfig.baseColorFactor;
        KU_INFO("MclarenPass: applied baseColorFactor from material config");
    }

    m_materialPathString = materialPath.string();
    m_materialConfigUsed = true;
}

void MclarenPass::loadSceneAndMaterialConfig()
{
    m_modelPathOverride.clear();
    m_sceneModelPaths.clear();
    m_sceneMaterialPaths.clear();
    m_scenePathString.clear();
    m_materialPathString.clear();
    m_sceneConfigUsed = false;
    m_materialConfigUsed = false;
    m_materialConfig = asset::MaterialConfig{};

    const std::filesystem::path scenePath = resolveScenePath();
    asset::SceneConfig sceneConfig{};
    std::string error;
    if (!asset::loadSceneConfigFromFile(scenePath, sceneConfig, &error)) {
        KU_WARN("MclarenPass: scene config fallback to hardcoded model path: {}", error);
        return;
    }

    m_cameraPosition = sceneConfig.camera.position;
    m_cameraTarget = sceneConfig.camera.target;
    m_cameraUp = sceneConfig.camera.up;
    m_cameraFovYDegrees = sceneConfig.camera.fovYDeg;
    m_cameraNear = sceneConfig.camera.nearPlane;
    m_cameraFar = sceneConfig.camera.farPlane;

    m_lightDirection = sceneConfig.lighting.direction;
    m_lightColor = sceneConfig.lighting.color;
    m_lightIntensity = sceneConfig.lighting.intensity;

    const float distance = glm::length(m_cameraPosition - m_cameraTarget);
    if (distance > 0.001f) {
        m_distance = distance;
    }

    const std::filesystem::path resourcesRoot = asset::findResourcesRoot(scenePath);
    if (resourcesRoot.empty()) {
        KU_WARN("MclarenPass: cannot resolve resources root from scene path: {}", scenePath.string());
    } else if (!sceneConfig.nodes.empty()) {
        for (const asset::SceneNodeConfig& node : sceneConfig.nodes) {
            if (!node.model.empty()) {
                const std::filesystem::path modelPath = resourcesRoot / node.model;
                if (std::filesystem::exists(modelPath)) {
                    m_sceneModelPaths.push_back(modelPath);
                } else {
                    KU_WARN("MclarenPass: model from scene config does not exist: {}", modelPath.string());
                }
            }

            if (!node.material.empty()) {
                const std::filesystem::path materialPath = resourcesRoot / node.material;
                if (std::filesystem::exists(materialPath)) {
                    m_sceneMaterialPaths.push_back(materialPath);
                } else {
                    KU_WARN("MclarenPass: material from scene config does not exist: {}", materialPath.string());
                }
            }
        }
    }

    if (!m_sceneModelPaths.empty() && m_sceneModelPaths.size() == 1u) {
        m_modelPathOverride = m_sceneModelPaths.front();
    }

    if (!m_sceneMaterialPaths.empty()) {
        loadMaterialConfig(m_sceneMaterialPaths.front());
        for (size_t i = 1; i < m_sceneMaterialPaths.size(); ++i) {
            if (m_sceneMaterialPaths[i] != m_sceneMaterialPaths.front()) {
                KU_WARN("MclarenPass: multiple material configs found; only the first is used: {}", m_sceneMaterialPaths.front().string());
                break;
            }
        }
    }

    m_scenePathString = scenePath.string();
    m_sceneConfigUsed = true;
}

void MclarenPass::initialize(RHIDevice& device)
{
    KU_INFO("MclarenPass: initializing...");

    m_deviceHandle = device.device();
    m_loadError.clear();
    destroyDescriptorResources();
    m_vertShader.reset();
    m_fragShader.reset();
    m_skyboxVertShader.reset();
    m_skyboxFragShader.reset();
    m_pipeline.reset();
    m_skyboxPipeline.reset();
    m_materialTextures.clear();
    m_materialBindings.clear();
    m_subMeshes.clear();
    m_fallbackWhiteTexture.reset();
    m_fallbackNormalTexture.reset();
    m_fallbackOrmTexture.reset();
    m_environmentTexture.reset();
    m_modelPathOverride.clear();
    m_scenePathString.clear();
    m_materialPathString.clear();
    m_environmentPathString.clear();
    m_sceneConfigUsed = false;
    m_materialConfigUsed = false;
    m_materialConfig = asset::MaterialConfig{};
    m_sceneModelPaths.clear();
    m_sceneMaterialPaths.clear();

    m_distance = 4.0f;
    m_cameraPosition = glm::vec3(0.0f, 0.0f, 4.0f);
    m_cameraTarget = glm::vec3(0.0f, 0.0f, 0.0f);
    m_cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);
    m_cameraFovYDegrees = 60.0f;
    m_cameraNear = 0.1f;
    m_cameraFar = 200.0f;
    m_lightDirection = glm::vec3(0.35f, 1.0f, 0.45f);
    m_lightColor = glm::vec3(1.0f, 1.0f, 1.0f);
    m_lightIntensity = 1.0f;
    m_enableEnvironmentMap = true;
    m_enableSkybox = true;
    m_environmentIntensity = 0.7f;
    m_environmentExposure = 1.0f;

    std::filesystem::path shaderDir = std::filesystem::current_path() / "shaders";
    const auto vertPath = shaderDir / "mclaren.vert.spv";
    const auto fragPath = shaderDir / "mclaren.frag.spv";
    const auto skyboxVertPath = shaderDir / "skybox.vert.spv";
    const auto skyboxFragPath = shaderDir / "skybox.frag.spv";

    try {
        m_vertShader = std::make_unique<RHIShader>(device, vertPath);
        m_fragShader = std::make_unique<RHIShader>(device, fragPath);
        m_skyboxVertShader = std::make_unique<RHIShader>(device, skyboxVertPath);
        m_skyboxFragShader = std::make_unique<RHIShader>(device, skyboxFragPath);
    } catch (const std::exception& e) {
        m_loadError = std::string("Shader load failed: ") + e.what();
        KU_ERROR("MclarenPass: {}", m_loadError);
        return;
    }

    loadSceneAndMaterialConfig();

    std::vector<std::filesystem::path> modelPaths = m_sceneModelPaths;
    if (modelPaths.empty()) {
        modelPaths.push_back(m_modelPathOverride.empty() ? resolveModelPath() : m_modelPathOverride);
    }

    if (modelPaths.size() == 1u) {
        m_modelPathString = modelPaths.front().string();
    } else {
        std::ostringstream oss;
        oss << "scene (" << modelPaths.size() << " models)";
        m_modelPathString = oss.str();
    }

    asset::MeshData mesh;
    bool loadedAny = false;
    for (const std::filesystem::path& modelPath : modelPaths) {
        try {
            asset::MeshData next = asset::ModelLoader::loadFromFile(modelPath);
            appendMeshData(next, mesh);
            loadedAny = true;
        } catch (const std::exception& e) {
            KU_WARN("MclarenPass: model load failed ({}): {}", modelPath.string(), e.what());
        }
    }

    if (!loadedAny) {
        m_loadError = "Model load failed: no scene models were loaded";
        KU_ERROR("MclarenPass: {}", m_loadError);
        return;
    }

    m_vertexCount = static_cast<uint32_t>(mesh.vertices.size());
    m_indexCount = static_cast<uint32_t>(mesh.indices.size());
    m_subMeshes = mesh.subMeshes;
    if (m_subMeshes.empty()) {
        m_subMeshes.push_back(asset::SubMeshData{0, m_indexCount, 0});
    }

    m_globalBaseColorFactor = {1.0f, 1.0f, 1.0f, 1.0f};

    m_modelCenter = 0.5f * (mesh.boundsMin + mesh.boundsMax);
    const float radius = 0.5f * glm::length(mesh.boundsMax - mesh.boundsMin);
    m_fitScale = radius > 1e-4f ? (1.5f / radius) : 1.0f;

    const VkDeviceSize vertexBytes = static_cast<VkDeviceSize>(mesh.vertices.size() * sizeof(asset::MeshVertex));
    const VkDeviceSize indexBytes = static_cast<VkDeviceSize>(mesh.indices.size() * sizeof(uint32_t));

    RHIBuffer::CreateInfo vbInfo{};
    vbInfo.size = vertexBytes;
    vbInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    vbInfo.memoryUsage = VMA_MEMORY_USAGE_AUTO;
    vbInfo.allocationFlags =
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;

    RHIBuffer::CreateInfo ibInfo{};
    ibInfo.size = indexBytes;
    ibInfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    ibInfo.memoryUsage = VMA_MEMORY_USAGE_AUTO;
    ibInfo.allocationFlags =
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;

    m_vertexBuffer = std::make_unique<RHIBuffer>(device, vbInfo);
    m_indexBuffer = std::make_unique<RHIBuffer>(device, ibInfo);

    void* vbMapped = m_vertexBuffer->map();
    void* ibMapped = m_indexBuffer->map();
    if (vbMapped == nullptr || ibMapped == nullptr) {
        m_loadError = "Failed to map vertex/index buffer memory";
        KU_ERROR("MclarenPass: {}", m_loadError);
        return;
    }

    std::memcpy(vbMapped, mesh.vertices.data(), static_cast<size_t>(vertexBytes));
    std::memcpy(ibMapped, mesh.indices.data(), static_cast<size_t>(indexBytes));

    m_vertexBuffer->flush();
    m_indexBuffer->flush();
    m_vertexBuffer->unmap();
    m_indexBuffer->unmap();

    VkVertexInputBindingDescription binding{};
    binding.binding = 0;
    binding.stride = sizeof(asset::MeshVertex);
    binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkVertexInputAttributeDescription attrPos{};
    attrPos.location = 0;
    attrPos.binding = 0;
    attrPos.format = VK_FORMAT_R32G32B32_SFLOAT;
    attrPos.offset = static_cast<uint32_t>(offsetof(asset::MeshVertex, position));

    VkVertexInputAttributeDescription attrNormal{};
    attrNormal.location = 1;
    attrNormal.binding = 0;
    attrNormal.format = VK_FORMAT_R32G32B32_SFLOAT;
    attrNormal.offset = static_cast<uint32_t>(offsetof(asset::MeshVertex, normal));

    VkVertexInputAttributeDescription attrUv0{};
    attrUv0.location = 2;
    attrUv0.binding = 0;
    attrUv0.format = VK_FORMAT_R32G32_SFLOAT;
    attrUv0.offset = static_cast<uint32_t>(offsetof(asset::MeshVertex, uv0));

    VkVertexInputAttributeDescription attrUv1{};
    attrUv1.location = 3;
    attrUv1.binding = 0;
    attrUv1.format = VK_FORMAT_R32G32_SFLOAT;
    attrUv1.offset = static_cast<uint32_t>(offsetof(asset::MeshVertex, uv1));

    VkVertexInputAttributeDescription attrTangent{};
    attrTangent.location = 4;
    attrTangent.binding = 0;
    attrTangent.format = VK_FORMAT_R32G32B32A32_SFLOAT;
    attrTangent.offset = static_cast<uint32_t>(offsetof(asset::MeshVertex, tangent));

    // 每个材质固定绑定 3 张采样纹理：BaseColor、Normal、ORM。
    std::array<VkDescriptorSetLayoutBinding, 3> textureBindings{};
    for (uint32_t i = 0; i < textureBindings.size(); ++i) {
        textureBindings[i].binding = i;
        textureBindings[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        textureBindings[i].descriptorCount = 1;
        textureBindings[i].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    }

    VkDescriptorSetLayoutCreateInfo setLayoutInfo{};
    setLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    setLayoutInfo.bindingCount = static_cast<uint32_t>(textureBindings.size());
    setLayoutInfo.pBindings = textureBindings.data();
    VK_CHECK(vkCreateDescriptorSetLayout(m_deviceHandle, &setLayoutInfo, nullptr, &m_descriptorSetLayout));

    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(device.physicalDevice(), &properties);

    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.maxAnisotropy = 1.0f;
    samplerInfo.anisotropyEnable = VK_FALSE;
    if (device.features().samplerAnisotropy) {
        samplerInfo.anisotropyEnable = VK_TRUE;
        samplerInfo.maxAnisotropy = std::min(8.0f, properties.limits.maxSamplerAnisotropy);
    }
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = 0.0f;
    samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    VK_CHECK(vkCreateSampler(m_deviceHandle, &samplerInfo, nullptr, &m_sampler));

    VkDescriptorSetLayoutBinding frameBinding{};
    frameBinding.binding = 0;
    frameBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    frameBinding.descriptorCount = 1;
    frameBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo frameSetLayoutInfo{};
    frameSetLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    frameSetLayoutInfo.bindingCount = 1;
    frameSetLayoutInfo.pBindings = &frameBinding;
    VK_CHECK(vkCreateDescriptorSetLayout(m_deviceHandle, &frameSetLayoutInfo, nullptr, &m_frameDescriptorSetLayout));

    VkDescriptorPoolSize framePoolSize{};
    framePoolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    framePoolSize.descriptorCount = 1;

    VkDescriptorPoolCreateInfo framePoolInfo{};
    framePoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    framePoolInfo.maxSets = 1;
    framePoolInfo.poolSizeCount = 1;
    framePoolInfo.pPoolSizes = &framePoolSize;
    VK_CHECK(vkCreateDescriptorPool(m_deviceHandle, &framePoolInfo, nullptr, &m_frameDescriptorPool));

    VkDescriptorSetAllocateInfo frameAllocInfo{};
    frameAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    frameAllocInfo.descriptorPool = m_frameDescriptorPool;
    frameAllocInfo.descriptorSetCount = 1;
    frameAllocInfo.pSetLayouts = &m_frameDescriptorSetLayout;
    VK_CHECK(vkAllocateDescriptorSets(m_deviceHandle, &frameAllocInfo, &m_frameDescriptorSet));

    RHIBuffer::CreateInfo frameUboInfo{};
    frameUboInfo.size = sizeof(FrameUniforms);
    frameUboInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    frameUboInfo.memoryUsage = VMA_MEMORY_USAGE_AUTO;
    frameUboInfo.allocationFlags =
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
    m_frameUniformBuffer = std::make_unique<RHIBuffer>(device, frameUboInfo);

    VkDescriptorBufferInfo frameBufferInfo{};
    frameBufferInfo.buffer = m_frameUniformBuffer->buffer();
    frameBufferInfo.offset = 0;
    frameBufferInfo.range = sizeof(FrameUniforms);

    VkWriteDescriptorSet frameWrite{};
    frameWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    frameWrite.dstSet = m_frameDescriptorSet;
    frameWrite.dstBinding = 0;
    frameWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    frameWrite.descriptorCount = 1;
    frameWrite.pBufferInfo = &frameBufferInfo;
    vkUpdateDescriptorSets(m_deviceHandle, 1, &frameWrite, 0, nullptr);

    VkDescriptorSetLayoutBinding envBinding{};
    envBinding.binding = 0;
    envBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    envBinding.descriptorCount = 1;
    envBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo envSetLayoutInfo{};
    envSetLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    envSetLayoutInfo.bindingCount = 1;
    envSetLayoutInfo.pBindings = &envBinding;
    VK_CHECK(vkCreateDescriptorSetLayout(m_deviceHandle, &envSetLayoutInfo, nullptr, &m_environmentDescriptorSetLayout));

    VkDescriptorPoolSize envPoolSize{};
    envPoolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    envPoolSize.descriptorCount = 1;

    VkDescriptorPoolCreateInfo envPoolInfo{};
    envPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    envPoolInfo.maxSets = 1;
    envPoolInfo.poolSizeCount = 1;
    envPoolInfo.pPoolSizes = &envPoolSize;
    VK_CHECK(vkCreateDescriptorPool(m_deviceHandle, &envPoolInfo, nullptr, &m_environmentDescriptorPool));

    VkDescriptorSetAllocateInfo envAllocInfo{};
    envAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    envAllocInfo.descriptorPool = m_environmentDescriptorPool;
    envAllocInfo.descriptorSetCount = 1;
    envAllocInfo.pSetLayouts = &m_environmentDescriptorSetLayout;
    VK_CHECK(vkAllocateDescriptorSets(m_deviceHandle, &envAllocInfo, &m_environmentDescriptorSet));

    const uint32_t materialCount = std::max(1u, static_cast<uint32_t>(mesh.materials.size()));

    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSize.descriptorCount = materialCount * 3u;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.maxSets = materialCount;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    VK_CHECK(vkCreateDescriptorPool(m_deviceHandle, &poolInfo, nullptr, &m_descriptorPool));

    // 为缺失贴图的材质准备 1x1 兜底纹理，避免采样空资源导致渲染异常。
    if (!createSolidColorTexture(device, {255, 255, 255, 255}, VK_FORMAT_R8G8B8A8_SRGB, m_fallbackWhiteTexture)) {
        m_loadError = "Fallback baseColor texture upload failed";
        KU_ERROR("MclarenPass: {}", m_loadError);
        return;
    }
    if (!createSolidColorTexture(device, {128, 128, 255, 255}, VK_FORMAT_R8G8B8A8_UNORM, m_fallbackNormalTexture)) {
        m_loadError = "Fallback normal texture upload failed";
        KU_ERROR("MclarenPass: {}", m_loadError);
        return;
    }
    if (!createSolidColorTexture(device, {255, 255, 0, 255}, VK_FORMAT_R8G8B8A8_UNORM, m_fallbackOrmTexture)) {
        m_loadError = "Fallback ORM texture upload failed";
        KU_ERROR("MclarenPass: {}", m_loadError);
        return;
    }

    const std::filesystem::path environmentPath = resolveEnvironmentPath();
    m_environmentPathString = environmentPath.string();
    if (!createAndUploadHdrTexture(device, environmentPath, m_environmentTexture)) {
        KU_WARN("MclarenPass: environment HDR load failed, fallback to white texture: {}", m_environmentPathString);
    }

    VkImageView environmentView =
        m_environmentTexture ? m_environmentTexture->imageView() : m_fallbackWhiteTexture->imageView();
    VkDescriptorImageInfo environmentImageInfo{};
    environmentImageInfo.sampler = m_sampler;
    environmentImageInfo.imageView = environmentView;
    environmentImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkWriteDescriptorSet environmentWrite{};
    environmentWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    environmentWrite.dstSet = m_environmentDescriptorSet;
    environmentWrite.dstBinding = 0;
    environmentWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    environmentWrite.descriptorCount = 1;
    environmentWrite.pImageInfo = &environmentImageInfo;
    vkUpdateDescriptorSets(m_deviceHandle, 1, &environmentWrite, 0, nullptr);

    std::vector<VkDescriptorSetLayout> layouts(materialCount, m_descriptorSetLayout);
    std::vector<VkDescriptorSet> descriptorSets(materialCount, VK_NULL_HANDLE);

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = m_descriptorPool;
    allocInfo.descriptorSetCount = materialCount;
    allocInfo.pSetLayouts = layouts.data();
    VK_CHECK(vkAllocateDescriptorSets(m_deviceHandle, &allocInfo, descriptorSets.data()));

    m_materialBindings.reserve(materialCount);
    m_materialTextures.reserve(materialCount * 3u);

    uint32_t texturedBase = 0;
    uint32_t texturedNormal = 0;
    uint32_t texturedOrm = 0;

    for (uint32_t i = 0; i < materialCount; ++i) {
        asset::MaterialData material{};
        if (i < mesh.materials.size()) {
            material = mesh.materials[i];
        }

        std::unique_ptr<RHITexture> baseTex;
        std::unique_ptr<RHITexture> normalTex;
        std::unique_ptr<RHITexture> ormTex;

        const asset::TextureData* baseSource = &material.baseColorTexture;
        const asset::TextureData* normalSource = &material.normalTexture;
        const asset::TextureData* ormSource = &material.ormTexture;

        if (m_materialConfigUsed) {
            if (m_materialConfig.baseColorBinding.hasSource) {
                const std::string sourceLower = toLower(m_materialConfig.baseColorBinding.source);
                if (isDisabledSource(sourceLower)) {
                    baseSource = nullptr;
                } else {
                    baseSource = resolveGltfTexture(m_materialConfig.baseColorBinding.source, material);
                }
                if (baseSource == nullptr && !isDisabledSource(sourceLower)) {
                    KU_WARN("MclarenPass: baseColor binding source not supported: {}", m_materialConfig.baseColorBinding.source);
                }
            }
            if (m_materialConfig.normalBinding.hasSource) {
                const std::string sourceLower = toLower(m_materialConfig.normalBinding.source);
                if (isDisabledSource(sourceLower)) {
                    normalSource = nullptr;
                } else {
                    normalSource = resolveGltfTexture(m_materialConfig.normalBinding.source, material);
                }
                if (normalSource == nullptr && !isDisabledSource(sourceLower)) {
                    KU_WARN("MclarenPass: normal binding source not supported: {}", m_materialConfig.normalBinding.source);
                }
            }
            if (m_materialConfig.ormBinding.hasSource) {
                const std::string sourceLower = toLower(m_materialConfig.ormBinding.source);
                if (isDisabledSource(sourceLower)) {
                    ormSource = nullptr;
                } else {
                    ormSource = resolveGltfTexture(m_materialConfig.ormBinding.source, material);
                }
                if (ormSource == nullptr && !isDisabledSource(sourceLower)) {
                    KU_WARN("MclarenPass: orm binding source not supported: {}", m_materialConfig.ormBinding.source);
                }
            }
        }

        const VkFormat baseFormat = formatForBinding(
            m_materialConfig.baseColorBinding,
            VK_FORMAT_R8G8B8A8_SRGB,
            "baseColor");
        const VkFormat normalFormat = formatForBinding(
            m_materialConfig.normalBinding,
            VK_FORMAT_R8G8B8A8_UNORM,
            "normal");
        const VkFormat ormFormat = formatForBinding(
            m_materialConfig.ormBinding,
            VK_FORMAT_R8G8B8A8_UNORM,
            "orm");

        // 尝试上传该材质对应纹理；上传失败或源纹理无效时会退回兜底纹理。
        bool hasBase = baseSource && baseSource->valid()
            && createAndUploadTexture(device, *baseSource, baseFormat, baseTex);
        bool hasNormal = normalSource && normalSource->valid()
            && createAndUploadTexture(device, *normalSource, normalFormat, normalTex);
        bool hasOrm = ormSource && ormSource->valid()
            && createAndUploadTexture(device, *ormSource, ormFormat, ormTex);

        // 默认先使用兜底视图，若上传成功再替换为真实纹理视图。
        VkImageView baseView = m_fallbackWhiteTexture->imageView();
        VkImageView normalView = m_fallbackNormalTexture->imageView();
        VkImageView ormView = m_fallbackOrmTexture->imageView();

        if (baseTex) {
            ++texturedBase;
            baseView = baseTex->imageView();
            m_materialTextures.push_back(std::move(baseTex));
        }
        if (normalTex) {
            ++texturedNormal;
            normalView = normalTex->imageView();
            m_materialTextures.push_back(std::move(normalTex));
        }
        if (ormTex) {
            ++texturedOrm;
            ormView = ormTex->imageView();
            m_materialTextures.push_back(std::move(ormTex));
        }

        // 将当前材质的三张纹理写入对应 descriptor set 的 0/1/2 号 binding。
        std::array<VkDescriptorImageInfo, 3> imageInfos{};
        imageInfos[0] = VkDescriptorImageInfo{m_sampler, baseView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
        imageInfos[1] = VkDescriptorImageInfo{m_sampler, normalView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
        imageInfos[2] = VkDescriptorImageInfo{m_sampler, ormView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};

        std::array<VkWriteDescriptorSet, 3> writes{};
        for (uint32_t bindingIndex = 0; bindingIndex < writes.size(); ++bindingIndex) {
            writes[bindingIndex].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[bindingIndex].dstSet = descriptorSets[i];
            writes[bindingIndex].dstBinding = bindingIndex;
            writes[bindingIndex].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            writes[bindingIndex].descriptorCount = 1;
            writes[bindingIndex].pImageInfo = &imageInfos[bindingIndex];
        }
        vkUpdateDescriptorSets(m_deviceHandle, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);

        // 把 glTF 材质参数拷贝到运行时结构，供每次 draw 时通过 push constants 使用。
        MaterialBinding materialBinding{};
        materialBinding.baseColorFactor = {
            material.baseColorFactor.r,
            material.baseColorFactor.g,
            material.baseColorFactor.b,
            material.baseColorFactor.a,
        };
        materialBinding.emissiveFactor = {
            material.emissiveFactor.x,
            material.emissiveFactor.y,
            material.emissiveFactor.z,
        };
        materialBinding.metallicFactor = material.metallicFactor;
        materialBinding.roughnessFactor = material.roughnessFactor;
        materialBinding.normalScale = material.normalScale;
        materialBinding.occlusionStrength = material.occlusionStrength;

        materialBinding.baseUvScaleOffset = {
            material.baseColorTransform.scale.x,
            material.baseColorTransform.scale.y,
            material.baseColorTransform.offset.x,
            material.baseColorTransform.offset.y,
        };
        materialBinding.normalUvScaleOffset = {
            material.normalTransform.scale.x,
            material.normalTransform.scale.y,
            material.normalTransform.offset.x,
            material.normalTransform.offset.y,
        };
        materialBinding.ormUvScaleOffset = {
            material.ormTransform.scale.x,
            material.ormTransform.scale.y,
            material.ormTransform.offset.x,
            material.ormTransform.offset.y,
        };

        materialBinding.baseUvRotation = material.baseColorTransform.rotation;
        materialBinding.normalUvRotation = material.normalTransform.rotation;
        materialBinding.ormUvRotation = material.ormTransform.rotation;

        materialBinding.baseTexCoord = clampTexCoordSet(material.baseColorTransform.texCoord);
        materialBinding.normalTexCoord = clampTexCoordSet(material.normalTransform.texCoord);
        materialBinding.ormTexCoord = clampTexCoordSet(material.ormTransform.texCoord);

        if (m_materialConfigUsed && m_materialConfig.baseColorBinding.hasUvSet) {
            materialBinding.baseTexCoord = clampTexCoordSet(static_cast<uint32_t>(m_materialConfig.baseColorBinding.uvSet));
        }
        if (m_materialConfigUsed && m_materialConfig.normalBinding.hasUvSet) {
            materialBinding.normalTexCoord = clampTexCoordSet(static_cast<uint32_t>(m_materialConfig.normalBinding.uvSet));
        }
        if (m_materialConfigUsed && m_materialConfig.ormBinding.hasUvSet) {
            materialBinding.ormTexCoord = clampTexCoordSet(static_cast<uint32_t>(m_materialConfig.ormBinding.uvSet));
        }

        materialBinding.hasBaseColorTexture = hasBase;
        materialBinding.hasNormalTexture = hasNormal;
        materialBinding.hasOrmTexture = hasOrm;
        materialBinding.descriptorSet = descriptorSets[i];

        m_materialBindings.push_back(materialBinding);
    }

    const float alphaMode = m_materialConfigUsed ? alphaModeToFlag(m_materialConfig) : 0.0f;

    GraphicsPipelineDesc desc{};
    desc.shaders.push_back(*m_vertShader);
    desc.shaders.push_back(*m_fragShader);
    desc.colorFormats = {kColorFormat};
    desc.vertexBindings = {binding};
    desc.vertexAttributes = {attrPos, attrNormal, attrUv0, attrUv1, attrTangent};
    desc.descriptorSetLayouts = {m_descriptorSetLayout, m_frameDescriptorSetLayout, m_environmentDescriptorSetLayout};
    desc.pushConstantRanges = {
        VkPushConstantRange{
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
            0,
            sizeof(PushConstants)}
    };
    desc.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    desc.cullMode = VK_CULL_MODE_NONE;
    desc.depthTest = true;
    desc.depthWrite = true;
    desc.depthFormat = m_depthFormat;
    desc.blendEnable = alphaMode >= 1.5f;

    m_pipeline = std::make_unique<RHIPipeline>(device, desc);

    GraphicsPipelineDesc skyboxDesc{};
    skyboxDesc.shaders.push_back(*m_skyboxVertShader);
    skyboxDesc.shaders.push_back(*m_skyboxFragShader);
    skyboxDesc.colorFormats = {kColorFormat};
    skyboxDesc.descriptorSetLayouts = {m_frameDescriptorSetLayout, m_environmentDescriptorSetLayout};
    skyboxDesc.pushConstantRanges = {
        VkPushConstantRange{
            VK_SHADER_STAGE_FRAGMENT_BIT,
            0,
            sizeof(SkyboxPushConstants)}
    };
    skyboxDesc.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    skyboxDesc.cullMode = VK_CULL_MODE_NONE;
    skyboxDesc.depthTest = false;
    skyboxDesc.depthWrite = false;
    skyboxDesc.depthFormat = m_depthFormat;

    m_skyboxPipeline = std::make_unique<RHIPipeline>(device, skyboxDesc);

    KU_INFO(
        "MclarenPass initialized: vertices={}, indices={}, materials={}, subMeshes={}, textured(base/normal/orm)=({}/{}/{}), model={}, environment={}",
        m_vertexCount,
        m_indexCount,
        m_materialBindings.size(),
        m_subMeshes.size(),
        texturedBase,
        texturedNormal,
        texturedOrm,
        m_modelPathString,
        m_environmentPathString);
}

void MclarenPass::execute(CommandList& cmd, const FrameData&)
{
    if (!m_pipeline || !m_vertexBuffer || !m_indexBuffer || m_indexCount == 0 || m_materialBindings.empty()) {
        return;
    }

    const float alphaMode = m_materialConfigUsed ? alphaModeToFlag(m_materialConfig) : 0.0f;

    glm::mat4 model = glm::mat4(1.0f);
    model = glm::rotate(model, m_yaw, glm::vec3(0.0f, 1.0f, 0.0f));
    model = glm::rotate(model, m_pitch, glm::vec3(1.0f, 0.0f, 0.0f));
    model = glm::scale(model, glm::vec3(m_fitScale));
    model = glm::translate(model, -m_modelCenter);

    glm::vec3 cameraForward = m_cameraPosition - m_cameraTarget;
    if (glm::length(cameraForward) < 1e-4f) {
        cameraForward = glm::vec3(0.0f, 0.0f, 1.0f);
    } else {
        cameraForward = glm::normalize(cameraForward);
    }

    glm::vec3 cameraUp = m_cameraUp;
    if (glm::length(cameraUp) < 1e-4f) {
        cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);
    } else {
        cameraUp = glm::normalize(cameraUp);
    }

    const glm::vec3 cameraPosition = m_cameraTarget + cameraForward * std::max(m_distance, 0.1f);

    const glm::mat4 view = glm::lookAt(
        cameraPosition,
        m_cameraTarget,
        cameraUp);

    const float safeNear = std::max(0.001f, m_cameraNear);
    const float safeFar = std::max(safeNear + 0.1f, m_cameraFar);
    const float safeFov = std::clamp(m_cameraFovYDegrees, 1.0f, 179.0f);
    glm::mat4 proj = glm::perspective(glm::radians(safeFov), std::max(m_aspect, 0.01f), safeNear, safeFar);
    proj[1][1] *= -1.0f;

    const glm::mat4 viewProj = proj * view;
    const glm::mat4 mvp = viewProj * model;
    const glm::mat4 invViewProj = glm::inverse(viewProj);
    const glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(model)));

    PushConstants push{};
    std::memcpy(push.mvp, &mvp[0][0], sizeof(push.mvp));

    for (int col = 0; col < 3; ++col) {
        // GLSL mat3(vec3, vec3, vec3) consumes column vectors.
        push.normalRows[col * 4 + 0] = normalMatrix[col][0];
        push.normalRows[col * 4 + 1] = normalMatrix[col][1];
        push.normalRows[col * 4 + 2] = normalMatrix[col][2];
        push.normalRows[col * 4 + 3] = m_lightColor[col];
    }

    glm::vec3 lightDirection = m_lightDirection;
    if (glm::length(lightDirection) < 1e-4f) {
        lightDirection = glm::vec3(0.35f, 1.0f, 0.45f);
    } else {
        lightDirection = glm::normalize(lightDirection);
    }
    FrameUniforms frame{};
    std::memcpy(frame.model, &model[0][0], sizeof(frame.model));
    frame.cameraPos[0] = cameraPosition.x;
    frame.cameraPos[1] = cameraPosition.y;
    frame.cameraPos[2] = cameraPosition.z;
    frame.cameraPos[3] = 1.0f;
    std::memcpy(frame.invViewProj, &invViewProj[0][0], sizeof(frame.invViewProj));
    frame.lightDirIntensity[0] = lightDirection.x;
    frame.lightDirIntensity[1] = lightDirection.y;
    frame.lightDirIntensity[2] = lightDirection.z;
    frame.lightDirIntensity[3] = std::max(0.0f, m_lightIntensity);
    frame.emissiveFactor[0] = 0.0f;
    frame.emissiveFactor[1] = 0.0f;
    frame.emissiveFactor[2] = 0.0f;
    frame.emissiveFactor[3] = 0.0f;
    frame.alphaParams[0] = 0.0f;
    frame.alphaParams[1] = 0.5f;
    frame.alphaParams[2] = 0.0f;
    frame.alphaParams[3] = 0.0f;

    if (m_frameUniformBuffer) {
        void* mapped = m_frameUniformBuffer->map();
        if (mapped != nullptr) {
            std::memcpy(mapped, &frame, sizeof(frame));
            m_frameUniformBuffer->flush();
            m_frameUniformBuffer->unmap();
        }
    }

    if (m_enableSkybox && m_skyboxPipeline && m_frameDescriptorSet != VK_NULL_HANDLE
        && m_environmentDescriptorSet != VK_NULL_HANDLE) {
        m_skyboxPipeline->bind(cmd);

        vkCmdBindDescriptorSets(
            cmd,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            m_skyboxPipeline->layout(),
            0,
            1,
            &m_frameDescriptorSet,
            0,
            nullptr);

        vkCmdBindDescriptorSets(
            cmd,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            m_skyboxPipeline->layout(),
            1,
            1,
            &m_environmentDescriptorSet,
            0,
            nullptr);

        SkyboxPushConstants skyboxPush{};
        skyboxPush.params[0] = std::max(0.0f, m_environmentExposure);
        skyboxPush.params[1] = m_enableOutputGamma ? 1.0f : 0.0f;
        skyboxPush.params[2] = 0.0f;
        skyboxPush.params[3] = 0.0f;

        vkCmdPushConstants(
            cmd,
            m_skyboxPipeline->layout(),
            VK_SHADER_STAGE_FRAGMENT_BIT,
            0,
            sizeof(SkyboxPushConstants),
            &skyboxPush);

        vkCmdDraw(cmd, 3, 1, 0, 0);
    }

    m_pipeline->bind(cmd);

    if (m_frameDescriptorSet != VK_NULL_HANDLE) {
        vkCmdBindDescriptorSets(
            cmd,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            m_pipeline->layout(),
            1,
            1,
            &m_frameDescriptorSet,
            0,
            nullptr);
    }

    if (m_environmentDescriptorSet != VK_NULL_HANDLE) {
        vkCmdBindDescriptorSets(
            cmd,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            m_pipeline->layout(),
            2,
            1,
            &m_environmentDescriptorSet,
            0,
            nullptr);
    }

    VkBuffer vertexBuffers[] = {m_vertexBuffer->buffer()};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(cmd, 0, 1, vertexBuffers, offsets);
    vkCmdBindIndexBuffer(cmd, m_indexBuffer->buffer(), 0, VK_INDEX_TYPE_UINT32);

    for (const asset::SubMeshData& subMesh : m_subMeshes) {
        size_t materialIndex = static_cast<size_t>(subMesh.materialIndex);
        if (materialIndex >= m_materialBindings.size()) {
            materialIndex = 0;
        }

        const MaterialBinding& material = m_materialBindings[materialIndex];

        const std::array<float, 4> combinedFactor = {
            material.baseColorFactor[0] * m_globalBaseColorFactor[0],
            material.baseColorFactor[1] * m_globalBaseColorFactor[1],
            material.baseColorFactor[2] * m_globalBaseColorFactor[2],
            material.baseColorFactor[3] * m_globalBaseColorFactor[3],
        };

        std::memcpy(push.baseColorFactor, combinedFactor.data(), sizeof(push.baseColorFactor));
        push.materialParams[0] = (m_enableTextureSampling && material.hasBaseColorTexture) ? 1.0f : 0.0f;
        push.materialParams[1] = m_flipUvY ? 1.0f : 0.0f;
        push.materialParams[2] = (m_enableNormalMap && material.hasNormalTexture) ? 1.0f : 0.0f;
        push.materialParams[3] = (m_enableOrmMap && material.hasOrmTexture) ? 1.0f : 0.0f;

        push.materialFactors[0] = material.normalScale;
        push.materialFactors[1] = material.metallicFactor;
        push.materialFactors[2] = material.roughnessFactor;
        push.materialFactors[3] = material.occlusionStrength;

        std::memcpy(push.baseUvScaleOffset, material.baseUvScaleOffset.data(), sizeof(push.baseUvScaleOffset));
        std::memcpy(push.normalUvScaleOffset, material.normalUvScaleOffset.data(), sizeof(push.normalUvScaleOffset));
        std::memcpy(push.ormUvScaleOffset, material.ormUvScaleOffset.data(), sizeof(push.ormUvScaleOffset));

        push.uvTransformParams0[0] = material.baseUvRotation;
        push.uvTransformParams0[1] = material.baseTexCoord;
        push.uvTransformParams0[2] = material.normalUvRotation;
        push.uvTransformParams0[3] = material.normalTexCoord;

        push.uvTransformParams1[0] = material.ormUvRotation;
        push.uvTransformParams1[1] = material.ormTexCoord;
        push.uvTransformParams1[2] = m_enableOutputGamma ? 1.0f : 0.0f;
        push.uvTransformParams1[3] =
            m_enableEnvironmentMap ? (std::max(0.0f, m_environmentIntensity) * std::max(0.0f, m_environmentExposure)) : 0.0f;

        frame.emissiveFactor[0] = material.emissiveFactor[0];
        frame.emissiveFactor[1] = material.emissiveFactor[1];
        frame.emissiveFactor[2] = material.emissiveFactor[2];
        frame.emissiveFactor[3] = 0.0f;
        frame.alphaParams[0] = alphaMode;
        frame.alphaParams[1] = m_materialConfigUsed ? m_materialConfig.alphaCutoff : 0.5f;
        frame.alphaParams[2] = 0.0f;
        frame.alphaParams[3] = 0.0f;

        if (m_frameUniformBuffer) {
            void* mapped = m_frameUniformBuffer->map();
            if (mapped != nullptr) {
                std::memcpy(mapped, &frame, sizeof(frame));
                m_frameUniformBuffer->flush();
                m_frameUniformBuffer->unmap();
            }
        }

        if (material.descriptorSet != VK_NULL_HANDLE) {
            vkCmdBindDescriptorSets(
                cmd,
                VK_PIPELINE_BIND_POINT_GRAPHICS,
                m_pipeline->layout(),
                0,
                1,
                &material.descriptorSet,
                0,
                nullptr);
        }

        vkCmdPushConstants(
            cmd,
            m_pipeline->layout(),
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
            0,
            sizeof(PushConstants),
            &push);

        vkCmdDrawIndexed(cmd, subMesh.indexCount, 1, subMesh.indexStart, 0, 0);
    }
}

void MclarenPass::drawUI()
{
    ImGui::Begin("Mclaren Model Controls");
    drawUIInline();
    ImGui::End();
}

void MclarenPass::drawUIInline()
{
    ImGuiIO& io = ImGui::GetIO();
    if (!io.WantCaptureMouse && io.MouseWheel != 0.0f) {
        const float zoomStep = std::max(0.1f, m_distance * 0.1f);
        m_distance = std::clamp(m_distance - io.MouseWheel * zoomStep, 2.0f, 12.0f);
    }

    ImGui::Text("Model: %s", m_modelPathString.c_str());
    ImGui::Text("Vertices: %u", m_vertexCount);
    ImGui::Text("Indices: %u", m_indexCount);
    ImGui::Text("SubMeshes: %u", static_cast<uint32_t>(m_subMeshes.size()));

    uint32_t texturedBase = 0;
    uint32_t texturedNormal = 0;
    uint32_t texturedOrm = 0;
    for (const MaterialBinding& material : m_materialBindings) {
        if (material.hasBaseColorTexture) {
            ++texturedBase;
        }
        if (material.hasNormalTexture) {
            ++texturedNormal;
        }
        if (material.hasOrmTexture) {
            ++texturedOrm;
        }
    }
    ImGui::Text(
        "Textured materials (base/normal/orm): %u / %u / %u",
        texturedBase,
        texturedNormal,
        texturedOrm);

    ImGui::Checkbox("Enable BaseColor Texture", &m_enableTextureSampling);
    ImGui::Checkbox("Enable Normal Map", &m_enableNormalMap);
    ImGui::Checkbox("Enable ORM Map", &m_enableOrmMap);
    ImGui::Checkbox("Flip UV-Y (Vulkan)", &m_flipUvY);
    ImGui::Checkbox("Encode Output Gamma (Linear->sRGB)", &m_enableOutputGamma);
    ImGui::ColorEdit4("Global BaseColor Factor", m_globalBaseColorFactor.data());

    ImGui::Separator();
    ImGui::Text("Camera");
    ImGui::Text("Camera Distance (Mouse Wheel): %.2f", m_distance);
    ImGui::TextDisabled("Use mouse wheel over viewport to zoom");
    ImGui::SliderFloat("Camera FOV Y", &m_cameraFovYDegrees, 20.0f, 120.0f);
    ImGui::SliderFloat("Camera Near", &m_cameraNear, 0.01f, 5.0f);
    ImGui::SliderFloat("Camera Far", &m_cameraFar, 5.0f, 500.0f);
    ImGui::Text("Yaw: %.2f", m_yaw);
    ImGui::Text("Pitch: %.2f", m_pitch);

    ImGui::Separator();
    ImGui::Text("Lighting");
    ImGui::SliderFloat3("Light Direction", &m_lightDirection.x, -1.0f, 1.0f);
    ImGui::ColorEdit3("Light Color", &m_lightColor.x);
    ImGui::SliderFloat("Light Intensity", &m_lightIntensity, 0.0f, 4.0f);
    ImGui::Checkbox("Enable Skybox", &m_enableSkybox);
    ImGui::Checkbox("Enable Environment Reflections", &m_enableEnvironmentMap);
    ImGui::SliderFloat("Environment Exposure", &m_environmentExposure, 0.1f, 4.0f);
    ImGui::SliderFloat("Environment Intensity", &m_environmentIntensity, 0.0f, 2.0f);

    ImGui::Separator();
    ImGui::TextWrapped("Scene Config: %s", m_sceneConfigUsed ? m_scenePathString.c_str() : "fallback (not found)");
    ImGui::TextWrapped(
        "Material Config: %s",
        m_materialConfigUsed ? m_materialPathString.c_str() : "fallback (not found)");
    ImGui::TextWrapped(
        "Environment HDR: %s",
        m_environmentPathString.empty() ? "fallback (not found)" : m_environmentPathString.c_str());

    if (!m_loadError.empty()) {
        ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.35f, 1.0f), "%s", m_loadError.c_str());
    }

    if (ImGui::Button("Reset Rotation")) {
        m_yaw = 0.0f;
        m_pitch = 0.0f;
    }
}

void MclarenPass::onResize(uint32_t width, uint32_t height)
{
    if (height == 0) {
        return;
    }

    m_aspect = static_cast<float>(width) / static_cast<float>(height);
}

void MclarenPass::addRotation(float deltaYaw, float deltaPitch)
{
    m_yaw += deltaYaw;
    m_pitch = std::clamp(m_pitch + deltaPitch, -1.5f, 1.5f);
}

bool MclarenPass::createAndUploadTexture(
    RHIDevice& device,
    const asset::TextureData& textureData,
    VkFormat format,
    std::unique_ptr<RHITexture>& outTexture)
{
    if (!textureData.valid()) {
        return false;
    }

    const VkDeviceSize textureBytes = static_cast<VkDeviceSize>(textureData.rgba8.size());

    // 先把 CPU 侧像素数据写入 staging buffer，再通过拷贝命令上传到 GPU 纹理。
    RHIBuffer::CreateInfo stagingInfo{};
    stagingInfo.size = textureBytes;
    stagingInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    stagingInfo.memoryUsage = VMA_MEMORY_USAGE_AUTO;
    stagingInfo.allocationFlags =
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;

    RHIBuffer staging(device, stagingInfo);
    void* mapped = staging.map();
    if (mapped == nullptr) {
        return false;
    }

    std::memcpy(mapped, textureData.rgba8.data(), static_cast<size_t>(textureBytes));
    staging.flush();
    staging.unmap();

    RHITexture::CreateInfo texInfo{};
    texInfo.width = textureData.width;
    texInfo.height = textureData.height;
    texInfo.format = format;
    texInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    texInfo.aspect = VK_IMAGE_ASPECT_COLOR_BIT;
    outTexture = std::make_unique<RHITexture>(device, texInfo);

    VkCommandPool transferPool = device.createCommandPool();
    CommandList transferCmd(device, transferPool);
    transferCmd.begin();

    // 布局转换：UNDEFINED -> TRANSFER_DST，准备接收拷贝。
    transferCmd.imageBarrier(
        outTexture->image(),
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_IMAGE_ASPECT_COLOR_BIT);

    transferCmd.copyBufferToImage(
        staging.buffer(),
        outTexture->image(),
        textureData.width,
        textureData.height);

    // 布局转换：TRANSFER_DST -> SHADER_READ_ONLY，供片段着色器采样。
    transferCmd.imageBarrier(
        outTexture->image(),
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        VK_IMAGE_ASPECT_COLOR_BIT);

    submitImmediate(transferCmd, device);
    vkDestroyCommandPool(device.device(), transferPool, nullptr);
    return true;
}

bool MclarenPass::createSolidColorTexture(
    RHIDevice& device,
    std::array<uint8_t, 4> rgba,
    VkFormat format,
    std::unique_ptr<RHITexture>& outTexture)
{
    asset::TextureData tex;
    tex.width = 1;
    tex.height = 1;
    tex.rgba8 = {rgba[0], rgba[1], rgba[2], rgba[3]};
    return createAndUploadTexture(device, tex, format, outTexture);
}

bool MclarenPass::createAndUploadHdrTexture(
    RHIDevice& device,
    const std::filesystem::path& hdrPath,
    std::unique_ptr<RHITexture>& outTexture)
{
    int width = 0;
    int height = 0;
    int channels = 0;

    stbi_set_flip_vertically_on_load(false);
    float* hdrPixels = stbi_loadf(hdrPath.string().c_str(), &width, &height, &channels, 4);
    if (hdrPixels == nullptr || width <= 0 || height <= 0) {
        return false;
    }

    const VkDeviceSize textureBytes =
        static_cast<VkDeviceSize>(static_cast<uint64_t>(width) * static_cast<uint64_t>(height) * 4ull * sizeof(float));

    RHIBuffer::CreateInfo stagingInfo{};
    stagingInfo.size = textureBytes;
    stagingInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    stagingInfo.memoryUsage = VMA_MEMORY_USAGE_AUTO;
    stagingInfo.allocationFlags =
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;

    RHIBuffer staging(device, stagingInfo);
    void* mapped = staging.map();
    if (mapped == nullptr) {
        stbi_image_free(hdrPixels);
        return false;
    }

    std::memcpy(mapped, hdrPixels, static_cast<size_t>(textureBytes));
    stbi_image_free(hdrPixels);
    staging.flush();
    staging.unmap();

    RHITexture::CreateInfo texInfo{};
    texInfo.width = static_cast<uint32_t>(width);
    texInfo.height = static_cast<uint32_t>(height);
    texInfo.format = VK_FORMAT_R32G32B32A32_SFLOAT;
    texInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    texInfo.aspect = VK_IMAGE_ASPECT_COLOR_BIT;
    outTexture = std::make_unique<RHITexture>(device, texInfo);

    VkCommandPool transferPool = device.createCommandPool();
    CommandList transferCmd(device, transferPool);
    transferCmd.begin();

    transferCmd.imageBarrier(
        outTexture->image(),
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_IMAGE_ASPECT_COLOR_BIT);

    transferCmd.copyBufferToImage(
        staging.buffer(),
        outTexture->image(),
        static_cast<uint32_t>(width),
        static_cast<uint32_t>(height));

    transferCmd.imageBarrier(
        outTexture->image(),
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        VK_IMAGE_ASPECT_COLOR_BIT);

    submitImmediate(transferCmd, device);
    vkDestroyCommandPool(device.device(), transferPool, nullptr);
    return true;
}

void MclarenPass::destroyDescriptorResources()
{
    m_skyboxPipeline.reset();
    m_frameUniformBuffer.reset();
    m_materialBindings.clear();
    m_materialTextures.clear();
    m_subMeshes.clear();
    m_fallbackWhiteTexture.reset();
    m_fallbackNormalTexture.reset();
    m_fallbackOrmTexture.reset();
    m_environmentTexture.reset();

    if (m_deviceHandle == VK_NULL_HANDLE) {
        return;
    }

    if (m_descriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(m_deviceHandle, m_descriptorPool, nullptr);
        m_descriptorPool = VK_NULL_HANDLE;
    }
    if (m_frameDescriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(m_deviceHandle, m_frameDescriptorPool, nullptr);
        m_frameDescriptorPool = VK_NULL_HANDLE;
    }
    if (m_environmentDescriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(m_deviceHandle, m_environmentDescriptorPool, nullptr);
        m_environmentDescriptorPool = VK_NULL_HANDLE;
    }
    if (m_sampler != VK_NULL_HANDLE) {
        vkDestroySampler(m_deviceHandle, m_sampler, nullptr);
        m_sampler = VK_NULL_HANDLE;
    }
    if (m_descriptorSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(m_deviceHandle, m_descriptorSetLayout, nullptr);
        m_descriptorSetLayout = VK_NULL_HANDLE;
    }
    if (m_frameDescriptorSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(m_deviceHandle, m_frameDescriptorSetLayout, nullptr);
        m_frameDescriptorSetLayout = VK_NULL_HANDLE;
    }
    if (m_environmentDescriptorSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(m_deviceHandle, m_environmentDescriptorSetLayout, nullptr);
        m_environmentDescriptorSetLayout = VK_NULL_HANDLE;
    }
    m_frameDescriptorSet = VK_NULL_HANDLE;
    m_environmentDescriptorSet = VK_NULL_HANDLE;
}

} // namespace ku
