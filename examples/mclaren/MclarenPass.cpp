#include "MclarenPass.h"

#include <KuEngine/Core/Log.h>
#include <KuEngine/Render/RenderGraph.h>

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <imgui.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <filesystem>
#include <vector>

namespace ku {

namespace {

constexpr VkFormat kColorFormat = VK_FORMAT_B8G8R8A8_UNORM;

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

void MclarenPass::initialize(RHIDevice& device)
{
    KU_INFO("MclarenPass: initializing...");

    m_deviceHandle = device.device();
    m_loadError.clear();
    destroyDescriptorResources();
    m_pipeline.reset();
    m_materialTextures.clear();
    m_materialBindings.clear();
    m_subMeshes.clear();
    m_fallbackWhiteTexture.reset();
    m_fallbackNormalTexture.reset();
    m_fallbackOrmTexture.reset();

    std::filesystem::path shaderDir = std::filesystem::current_path() / "shaders";
    const auto vertPath = shaderDir / "mclaren.vert.spv";
    const auto fragPath = shaderDir / "mclaren.frag.spv";

    try {
        m_vertShader = std::make_unique<RHIShader>(device, vertPath);
        m_fragShader = std::make_unique<RHIShader>(device, fragPath);
    } catch (const std::exception& e) {
        m_loadError = std::string("Shader load failed: ") + e.what();
        KU_ERROR("MclarenPass: {}", m_loadError);
        return;
    }

    const std::filesystem::path modelPath = resolveModelPath();
    m_modelPathString = modelPath.string();

    asset::MeshData mesh;
    try {
        mesh = asset::ModelLoader::loadFromFile(modelPath);
    } catch (const std::exception& e) {
        m_loadError = std::string("Model load failed: ") + e.what();
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

        bool hasBase = material.baseColorTexture.valid()
            && createAndUploadTexture(device, material.baseColorTexture, VK_FORMAT_R8G8B8A8_SRGB, baseTex);
        bool hasNormal = material.normalTexture.valid()
            && createAndUploadTexture(device, material.normalTexture, VK_FORMAT_R8G8B8A8_UNORM, normalTex);
        bool hasOrm = material.ormTexture.valid()
            && createAndUploadTexture(device, material.ormTexture, VK_FORMAT_R8G8B8A8_UNORM, ormTex);

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

        MaterialBinding materialBinding{};
        materialBinding.baseColorFactor = {
            material.baseColorFactor.r,
            material.baseColorFactor.g,
            material.baseColorFactor.b,
            material.baseColorFactor.a,
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

        materialBinding.hasBaseColorTexture = hasBase;
        materialBinding.hasNormalTexture = hasNormal;
        materialBinding.hasOrmTexture = hasOrm;
        materialBinding.descriptorSet = descriptorSets[i];

        m_materialBindings.push_back(materialBinding);
    }

    GraphicsPipelineDesc desc{};
    desc.shaders.push_back(*m_vertShader);
    desc.shaders.push_back(*m_fragShader);
    desc.colorFormats = {kColorFormat};
    desc.vertexBindings = {binding};
    desc.vertexAttributes = {attrPos, attrNormal, attrUv0, attrUv1};
    desc.descriptorSetLayouts = {m_descriptorSetLayout};
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

    m_pipeline = std::make_unique<RHIPipeline>(device, desc);

    KU_INFO(
        "MclarenPass initialized: vertices={}, indices={}, materials={}, subMeshes={}, textured(base/normal/orm)=({}/{}/{}), model={}",
        m_vertexCount,
        m_indexCount,
        m_materialBindings.size(),
        m_subMeshes.size(),
        texturedBase,
        texturedNormal,
        texturedOrm,
        m_modelPathString);
}

void MclarenPass::execute(CommandList& cmd, const FrameData&)
{
    if (!m_pipeline || !m_vertexBuffer || !m_indexBuffer || m_indexCount == 0 || m_materialBindings.empty()) {
        return;
    }

    glm::mat4 model = glm::mat4(1.0f);
    model = glm::rotate(model, m_yaw, glm::vec3(0.0f, 1.0f, 0.0f));
    model = glm::rotate(model, m_pitch, glm::vec3(1.0f, 0.0f, 0.0f));
    model = glm::scale(model, glm::vec3(m_fitScale));
    model = glm::translate(model, -m_modelCenter);

    const glm::mat4 view = glm::lookAt(
        glm::vec3(0.0f, 0.0f, m_distance),
        glm::vec3(0.0f, 0.0f, 0.0f),
        glm::vec3(0.0f, 1.0f, 0.0f));

    glm::mat4 proj = glm::perspective(glm::radians(60.0f), std::max(m_aspect, 0.01f), 0.1f, 200.0f);
    proj[1][1] *= -1.0f;

    const glm::mat4 mvp = proj * view * model;
    const glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(model)));

    PushConstants push{};
    std::memcpy(push.mvp, &mvp[0][0], sizeof(push.mvp));

    for (int row = 0; row < 3; ++row) {
        push.normalRows[row * 4 + 0] = normalMatrix[0][row];
        push.normalRows[row * 4 + 1] = normalMatrix[1][row];
        push.normalRows[row * 4 + 2] = normalMatrix[2][row];
        push.normalRows[row * 4 + 3] = 0.0f;
    }

    m_pipeline->bind(cmd);

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
        push.uvTransformParams1[2] = 0.0f;
        push.uvTransformParams1[3] = 0.0f;

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
    ImGui::ColorEdit4("Global BaseColor Factor", m_globalBaseColorFactor.data());
    ImGui::SliderFloat("Camera Distance", &m_distance, 2.0f, 12.0f);
    ImGui::Text("Yaw: %.2f", m_yaw);
    ImGui::Text("Pitch: %.2f", m_pitch);

    if (!m_loadError.empty()) {
        ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.35f, 1.0f), "%s", m_loadError.c_str());
    }

    if (ImGui::Button("Reset Rotation")) {
        m_yaw = 0.0f;
        m_pitch = 0.0f;
    }

    ImGui::End();
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

void MclarenPass::destroyDescriptorResources()
{
    m_materialBindings.clear();
    m_materialTextures.clear();
    m_subMeshes.clear();
    m_fallbackWhiteTexture.reset();
    m_fallbackNormalTexture.reset();
    m_fallbackOrmTexture.reset();

    if (m_deviceHandle == VK_NULL_HANDLE) {
        return;
    }

    if (m_descriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(m_deviceHandle, m_descriptorPool, nullptr);
        m_descriptorPool = VK_NULL_HANDLE;
    }
    if (m_sampler != VK_NULL_HANDLE) {
        vkDestroySampler(m_deviceHandle, m_sampler, nullptr);
        m_sampler = VK_NULL_HANDLE;
    }
    if (m_descriptorSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(m_deviceHandle, m_descriptorSetLayout, nullptr);
        m_descriptorSetLayout = VK_NULL_HANDLE;
    }
}

} // namespace ku
