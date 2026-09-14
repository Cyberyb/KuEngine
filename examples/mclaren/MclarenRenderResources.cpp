#include "MclarenRenderResources.h"

#include "MclarenSceneAsset.h"

#include <KuEngine/Core/Log.h>
#include <KuEngine/RHI/CommandList.h>
#include <KuEngine/RHI/RHIBuffer.h>
#include <KuEngine/RHI/RHIDevice.h>
#include <KuEngine/RHI/RHIPipeline.h>
#include <KuEngine/RHI/RHIShader.h>
#include <KuEngine/Render/GpuMesh.h>
#include <KuEngine/Render/GpuModelAsset.h>
#include <KuEngine/Render/PBRRenderer.h>
#include <KuEngine/Render/PBRResources.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstring>
#include <filesystem>
#include <limits>
#include <stdexcept>

namespace ku {

MclarenRenderResources::MclarenRenderResources() = default;

MclarenRenderResources::~MclarenRenderResources()
{
    reset();
}

bool MclarenRenderResources::initialize(
    RHIDevice& device,
    const MclarenSceneAsset& scene,
    GpuModelAsset& model,
    PBRMaterialResources& materials,
    PBREnvironmentResources& environment,
    VkFormat colorFormat,
    VkFormat depthFormat,
    VkCompareOp depthCompareOp,
    std::string& errorMessage)
{
    reset();
    errorMessage.clear();
    m_deviceHandle = device.device();
    m_model = &model;
    m_materials = &materials;
    m_environment = &environment;

    try {
        if (!model.ready() || !materials.ready() || !environment.ready()) {
            throw std::invalid_argument(
                "Mclaren assembly requires ready public GPU assets");
        }

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

        createFrameDescriptorLayout();
        createFrameResources(device, model.mesh().subMeshes().size());
        createPipelines(
            device,
            scene,
            colorFormat,
            depthFormat,
            depthCompareOp);
        configurePbrRenderer(depthFormat);
        if (!ready()) {
            throw std::runtime_error(
                "Mclaren GPU assembly did not reach the ready state");
        }
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
        model.mesh().vertexCount(),
        model.mesh().indexCount(),
        materials.bindings().size(),
        model.mesh().subMeshes().size(),
        materials.stats().texturedBase,
        materials.stats().texturedNormal,
        materials.stats().texturedOrm,
        materials.stats().texturedEmissive);
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
    destroyVulkanHandles();
    m_model = nullptr;
    m_materials = nullptr;
    m_environment = nullptr;
}

bool MclarenRenderResources::ready() const
{
    return m_pipeline != nullptr
        && m_skyboxPipeline != nullptr
        && m_pbrRenderer != nullptr
        && m_model != nullptr
        && m_model->ready()
        && m_materials != nullptr
        && m_materials->ready()
        && m_environment != nullptr
        && m_environment->ready();
}

PBRRenderer& MclarenRenderResources::pbrRenderer() const
{
    if (!m_pbrRenderer) {
        throw std::runtime_error("Mclaren PBR renderer is not initialized");
    }
    return *m_pbrRenderer;
}

void MclarenRenderResources::createFrameDescriptorLayout()
{
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
        >= std::numeric_limits<uint32_t>::max() / m_frameUniformStride) {
        throw std::runtime_error(
            "PBR frame uniform buffer exceeds Vulkan dynamic offset range");
    }

    RHIBuffer::CreateInfo bufferInfo{};
    bufferInfo.size = static_cast<VkDeviceSize>(m_frameUniformStride)
        * static_cast<VkDeviceSize>(actualDrawCount + 1);
    bufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    bufferInfo.memoryUsage = VMA_MEMORY_USAGE_AUTO;
    bufferInfo.allocationFlags =
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT
        | VMA_ALLOCATION_CREATE_MAPPED_BIT;
    m_frameUniformBuffer = std::make_unique<RHIBuffer>(device, bufferInfo);

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
    vkUpdateDescriptorSets(m_deviceHandle, 1, &write, 0, nullptr);
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
    attributes[0] = {0, 0, VK_FORMAT_R32G32B32_SFLOAT,
        static_cast<uint32_t>(offsetof(asset::MeshVertex, position))};
    attributes[1] = {1, 0, VK_FORMAT_R32G32B32_SFLOAT,
        static_cast<uint32_t>(offsetof(asset::MeshVertex, normal))};
    attributes[2] = {2, 0, VK_FORMAT_R32G32_SFLOAT,
        static_cast<uint32_t>(offsetof(asset::MeshVertex, uv0))};
    attributes[3] = {3, 0, VK_FORMAT_R32G32_SFLOAT,
        static_cast<uint32_t>(offsetof(asset::MeshVertex, uv1))};
    attributes[4] = {4, 0, VK_FORMAT_R32G32B32A32_SFLOAT,
        static_cast<uint32_t>(offsetof(asset::MeshVertex, tangent))};

    if (sizeof(PBRPushConstants)
        > device.properties().limits.maxPushConstantsSize) {
        throw std::runtime_error(
            "PBR push constants exceed the selected device maxPushConstantsSize");
    }

    GraphicsPipelineDesc pbrDesc{};
    pbrDesc.shaders = {*m_vertShader, *m_fragShader};
    pbrDesc.colorFormats = {colorFormat};
    pbrDesc.vertexBindings = {vertexBinding};
    pbrDesc.vertexAttributes.assign(attributes.begin(), attributes.end());
    pbrDesc.descriptorSetLayouts = {
        m_materials->descriptorSetLayout(),
        m_frameDescriptorSetLayout,
        m_environment->descriptorSetLayout(),
    };
    pbrDesc.pushConstantRanges = {
        VkPushConstantRange{
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
            0,
            sizeof(PBRPushConstants),
        },
    };
    pbrDesc.cullMode = VK_CULL_MODE_NONE;
    pbrDesc.depthTest = true;
    pbrDesc.depthWrite = true;
    pbrDesc.depthFormat = depthFormat;
    pbrDesc.depthCompareOp = depthCompareOp;
    pbrDesc.blendEnable = scene.materialConfigUsed()
        && alphaModeToFlag(scene.materialConfig()) >= 1.5f;
    m_pipeline = std::make_unique<RHIPipeline>(device, pbrDesc);

    GraphicsPipelineDesc skyboxDesc{};
    skyboxDesc.shaders = {*m_skyboxVertShader, *m_skyboxFragShader};
    skyboxDesc.colorFormats = {colorFormat};
    skyboxDesc.descriptorSetLayouts = {
        m_frameDescriptorSetLayout,
        m_environment->descriptorSetLayout(),
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
    m_skyboxPipeline = std::make_unique<RHIPipeline>(device, skyboxDesc);
}

void MclarenRenderResources::configurePbrRenderer(VkFormat depthFormat)
{
    GpuMesh& mesh = m_model->mesh();
    m_pbrRenderer = std::make_unique<PBRRenderer>();
    m_pbrRenderer->setDepthFormat(depthFormat);
    m_pbrRenderer->setPipeline(m_pipeline.get());
    m_pbrRenderer->setFrameDescriptorSet(m_frameDescriptorSet);
    m_pbrRenderer->setEnvironmentDescriptorSet(
        m_environment->descriptorSet());
    m_pbrRenderer->setMaterialBindings(&m_materials->bindings());
    m_pbrRenderer->setVertexIndexBuffers(
        &mesh.vertexBuffer(),
        &mesh.indexBuffer());
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
    if (!m_skyboxPipeline || m_frameDescriptorSet == VK_NULL_HANDLE
        || m_environment == nullptr || !m_environment->ready()) {
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
    const VkDescriptorSet environmentSet = m_environment->descriptorSet();
    vkCmdBindDescriptorSets(
        cmd,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        m_skyboxPipeline->layout(),
        1,
        1,
        &environmentSet,
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
    cmd.draw(3);
}

void MclarenRenderResources::destroyVulkanHandles()
{
    if (m_deviceHandle == VK_NULL_HANDLE) {
        return;
    }
    if (m_frameDescriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(
            m_deviceHandle,
            m_frameDescriptorPool,
            nullptr);
        m_frameDescriptorPool = VK_NULL_HANDLE;
    }
    if (m_frameDescriptorSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(
            m_deviceHandle,
            m_frameDescriptorSetLayout,
            nullptr);
        m_frameDescriptorSetLayout = VK_NULL_HANDLE;
    }
    m_frameDescriptorSet = VK_NULL_HANDLE;
    m_deviceHandle = VK_NULL_HANDLE;
}

} // namespace ku
