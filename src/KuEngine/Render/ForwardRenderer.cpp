#include "ForwardRenderer.h"

#include <KuEngine/Asset/Scene.h>
#include <KuEngine/RHI/CommandList.h>
#include <KuEngine/RHI/RHIBuffer.h>
#include <KuEngine/RHI/RHIDevice.h>
#include <KuEngine/RHI/RHIPipeline.h>
#include <KuEngine/Render/ForwardProgram.h>
#include <KuEngine/Render/GpuMesh.h>
#include <KuEngine/Render/GpuModelAsset.h>
#include <KuEngine/Render/PBRResources.h>

#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

namespace ku {
namespace {

std::unique_ptr<RHIBuffer> createHostUniformBuffer(
    RHIDevice& device, VkDeviceSize size)
{
    RHIBuffer::CreateInfo info{};
    info.size = size;
    info.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    info.memoryUsage = VMA_MEMORY_USAGE_AUTO;
    info.allocationFlags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT
        | VMA_ALLOCATION_CREATE_MAPPED_BIT;
    return std::make_unique<RHIBuffer>(device, info);
}

VkDescriptorPool createUniformPool(VkDevice device, VkDescriptorType type)
{
    VkDescriptorPoolSize size{type, 1};
    VkDescriptorPoolCreateInfo info{
        VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    info.maxSets = 1;
    info.poolSizeCount = 1;
    info.pPoolSizes = &size;
    VkDescriptorPool pool = VK_NULL_HANDLE;
    VK_CHECK(vkCreateDescriptorPool(device, &info, nullptr, &pool));
    return pool;
}

VkDescriptorSet allocateSet(
    VkDevice device, VkDescriptorPool pool, VkDescriptorSetLayout layout)
{
    VkDescriptorSetAllocateInfo info{
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    info.descriptorPool = pool;
    info.descriptorSetCount = 1;
    info.pSetLayouts = &layout;
    VkDescriptorSet set = VK_NULL_HANDLE;
    VK_CHECK(vkAllocateDescriptorSets(device, &info, &set));
    return set;
}

void updateUniformSet(
    VkDevice device, VkDescriptorSet set, VkDescriptorType type,
    VkBuffer buffer, VkDeviceSize range)
{
    VkDescriptorBufferInfo bufferInfo{buffer, 0, range};
    VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    write.dstSet = set;
    write.dstBinding = 0;
    write.descriptorType = type;
    write.descriptorCount = 1;
    write.pBufferInfo = &bufferInfo;
    vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);
}

} // namespace

struct ForwardRenderer::DynamicCandidate {
    VkDevice device = VK_NULL_HANDLE;
    std::unique_ptr<RHIBuffer> buffer;
    ForwardDynamicBufferLayout layout{};
    VkDescriptorPool pool = VK_NULL_HANDLE;
    VkDescriptorSet set = VK_NULL_HANDLE;
    ~DynamicCandidate()
    {
        if (device != VK_NULL_HANDLE && pool != VK_NULL_HANDLE) {
            vkDestroyDescriptorPool(device, pool, nullptr);
        }
    }
};

ForwardRenderer::ForwardRenderer() = default;

ForwardRenderer::~ForwardRenderer() { reset(); }

bool ForwardRenderer::initialize(
    RHIDevice& device, ForwardProgram& program, size_t initialCapacity,
    std::string& errorMessage)
{
    reset();
    errorMessage.clear();
    m_device = &device;
    m_program = &program;
    m_deviceHandle = device.device();
    try {
        if (!program.ready()) {
            throw std::invalid_argument("ForwardRenderer requires a ready program");
        }
        m_frameBuffer = createHostUniformBuffer(
            device, sizeof(ForwardFrameUniforms));
        m_framePool = createUniformPool(
            m_deviceHandle, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
        m_frameSet = allocateSet(m_deviceHandle, m_framePool,
            program.setLayout(forward_set::frame));
        updateUniformSet(m_deviceHandle, m_frameSet,
            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, m_frameBuffer->buffer(),
            sizeof(ForwardFrameUniforms));
        if (!ensureCapacity(std::max<size_t>(1, initialCapacity), errorMessage)) {
            throw std::runtime_error(errorMessage);
        }
    } catch (const std::exception& error) {
        errorMessage = error.what();
        reset();
        return false;
    } catch (...) {
        errorMessage = "Unknown ForwardRenderer initialization failure";
        reset();
        return false;
    }
    return true;
}

bool ForwardRenderer::ensureCapacity(size_t drawCapacity, std::string& errorMessage)
{
    errorMessage.clear();
    if (m_device == nullptr || m_program == nullptr
        || m_deviceHandle == VK_NULL_HANDLE) {
        errorMessage = "ForwardRenderer capacity requested before initialization";
        return false;
    }
    if (drawCapacity <= m_layout.capacity && m_drawBuffer != nullptr) {
        return true;
    }
    try {
        DynamicCandidate candidate{};
        candidate.device = m_deviceHandle;
        if (!calculateForwardDynamicBufferLayout(
                std::max<size_t>(1, drawCapacity),
                sizeof(ForwardDrawUniforms),
                m_device->properties().limits.minUniformBufferOffsetAlignment,
                candidate.layout)) {
            throw std::overflow_error(
                "Forward draw UBO capacity exceeds Vulkan dynamic-offset limits");
        }
        candidate.buffer = createHostUniformBuffer(
            *m_device, candidate.layout.totalSize);
        candidate.pool = createUniformPool(
            m_deviceHandle, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC);
        candidate.set = allocateSet(m_deviceHandle, candidate.pool,
            m_program->setLayout(forward_set::draw));
        updateUniformSet(m_deviceHandle, candidate.set,
            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
            candidate.buffer->buffer(), sizeof(ForwardDrawUniforms));

        if (m_drawPool != VK_NULL_HANDLE) {
            vkDestroyDescriptorPool(m_deviceHandle, m_drawPool, nullptr);
        }
        m_drawBuffer = std::move(candidate.buffer);
        m_layout = candidate.layout;
        m_drawPool = std::exchange(candidate.pool, VK_NULL_HANDLE);
        m_drawSet = candidate.set;
    } catch (const std::exception& error) {
        errorMessage = error.what();
        return false;
    } catch (...) {
        errorMessage = "Unknown ForwardRenderer capacity allocation failure";
        return false;
    }
    return true;
}

bool ForwardRenderer::writeFrame(const ForwardView& view)
{
    ForwardFrameUniforms uniforms{};
    std::memcpy(uniforms.viewProjection, glm::value_ptr(view.viewProjection),
        sizeof(uniforms.viewProjection));
    std::memcpy(uniforms.inverseViewProjection,
        glm::value_ptr(view.inverseViewProjection),
        sizeof(uniforms.inverseViewProjection));
    uniforms.cameraPosition[0] = view.cameraPosition.x;
    uniforms.cameraPosition[1] = view.cameraPosition.y;
    uniforms.cameraPosition[2] = view.cameraPosition.z;

    asset::SceneLightingConfig lighting = view.lighting;
    asset::sanitizeSceneLighting(lighting);
    uniforms.directionalDirectionIntensity[0] = lighting.direction.x;
    uniforms.directionalDirectionIntensity[1] = lighting.direction.y;
    uniforms.directionalDirectionIntensity[2] = lighting.direction.z;
    uniforms.directionalDirectionIntensity[3] = lighting.intensity;
    uniforms.directionalColorPointCount[0] = lighting.color.r;
    uniforms.directionalColorPointCount[1] = lighting.color.g;
    uniforms.directionalColorPointCount[2] = lighting.color.b;
    uniforms.directionalColorPointCount[3] =
        static_cast<float>(lighting.pointLights.size());
    for (size_t index = 0; index < lighting.pointLights.size(); ++index) {
        const auto& point = lighting.pointLights[index];
        uniforms.pointPositionRange[index][0] = point.position.x;
        uniforms.pointPositionRange[index][1] = point.position.y;
        uniforms.pointPositionRange[index][2] = point.position.z;
        uniforms.pointPositionRange[index][3] = point.range;
        uniforms.pointColorIntensity[index][0] = point.color.r;
        uniforms.pointColorIntensity[index][1] = point.color.g;
        uniforms.pointColorIntensity[index][2] = point.color.b;
        uniforms.pointColorIntensity[index][3] = point.intensity;
    }
    uniforms.environmentParams[0] = view.environmentEnabled ? 1.0f : 0.0f;
    uniforms.environmentParams[1] = std::max(0.0f, view.environmentIntensity);
    uniforms.environmentParams[2] = std::max(0.0f, view.environmentExposure);
    uniforms.environmentParams[3] = view.outputGamma ? 1.0f : 0.0f;
    void* mapped = m_frameBuffer->map();
    if (mapped == nullptr) return false;
    std::memcpy(mapped, &uniforms, sizeof(uniforms));
    m_frameBuffer->flush();
    m_frameBuffer->unmap();
    return true;
}

bool ForwardRenderer::writeDraws(
    std::span<const ForwardDraw> draws,
    bool outputGamma,
    float exposure)
{
    auto* mapped = static_cast<std::byte*>(m_drawBuffer->map());
    if (mapped == nullptr) return false;
    for (size_t index = 0; index < draws.size(); ++index) {
        const ForwardDraw& draw = draws[index];
        ForwardDrawUniforms uniforms{};
        std::memcpy(uniforms.model, glm::value_ptr(draw.modelMatrix),
            sizeof(uniforms.model));
        const glm::mat3 normal = asset::sceneNormalMatrix(draw.modelMatrix);
        for (int column = 0; column < 3; ++column) {
            uniforms.normalRows[column * 4] = normal[column].x;
            uniforms.normalRows[column * 4 + 1] = normal[column].y;
            uniforms.normalRows[column * 4 + 2] = normal[column].z;
        }
        for (size_t component = 0; component < 4; ++component) {
            uniforms.baseColorFactor[component] =
                draw.material.baseColorFactor[component] * draw.tint[component];
        }
        for (size_t component = 0; component < 3; ++component) {
            uniforms.emissiveFactor[component] = draw.material.emissiveFactor[component];
        }
        uniforms.materialFactors[0] = draw.material.metallicFactor;
        uniforms.materialFactors[1] = draw.material.roughnessFactor;
        uniforms.materialFactors[2] = draw.material.normalScale;
        uniforms.materialFactors[3] = draw.material.occlusionStrength;
        const MaterialGpuBinding& gpuMaterial =
            draw.materials->material(draw.materialIndex);
        const bool present[5] = {
            gpuMaterial.hasBaseColorTexture,
            gpuMaterial.hasNormalTexture,
            gpuMaterial.hasMetallicRoughnessTexture,
            gpuMaterial.hasOcclusionTexture,
            gpuMaterial.hasEmissiveTexture};
        for (size_t texture = 0; texture < 5; ++texture) {
            const auto& transform = draw.material.textureTransforms[texture];
            uniforms.textureScaleOffset[texture][0] = transform.scale.x;
            uniforms.textureScaleOffset[texture][1] = transform.scale.y;
            uniforms.textureScaleOffset[texture][2] = transform.offset.x;
            uniforms.textureScaleOffset[texture][3] = transform.offset.y;
            uniforms.textureRotationTexCoord[texture][0] = transform.rotation;
            uniforms.textureRotationTexCoord[texture][1] =
                clampTexCoordSet(transform.texCoord);
            uniforms.textureRotationTexCoord[texture][2] =
                present[texture] && draw.textureEnabled[texture] ? 1.0f : 0.0f;
        }
        uniforms.flags[0] = draw.material.alphaCutoff;
        uniforms.flags[1] = static_cast<float>(draw.material.alphaMode);
        uniforms.flags[2] = std::max(0.0f, exposure);
        uniforms.flags[3] = outputGamma ? 1.0f : 0.0f;
        std::memcpy(mapped + index * static_cast<size_t>(m_layout.stride),
            &uniforms, sizeof(uniforms));
    }
    m_drawBuffer->flush();
    m_drawBuffer->unmap();
    return true;
}

void ForwardRenderer::drawSkybox(CommandList& commandList,
    const ForwardView& view) const
{
    if (!view.skyboxEnabled || view.environment == nullptr
        || !view.environment->ready()) return;
    RHIPipeline* pipeline = m_program->skyboxPipeline();
    pipeline->bind(commandList);
    vkCmdBindDescriptorSets(commandList, VK_PIPELINE_BIND_POINT_GRAPHICS,
        pipeline->layout(), forward_set::frame, 1, &m_frameSet, 0, nullptr);
    const VkDescriptorSet environmentSet = view.environment->descriptorSet();
    vkCmdBindDescriptorSets(commandList, VK_PIPELINE_BIND_POINT_GRAPHICS,
        pipeline->layout(), forward_set::environment, 1, &environmentSet,
        0, nullptr);
    ForwardSkyboxPushConstants push{};
    push.params[0] = std::max(0.0f, view.environmentExposure);
    push.params[1] = view.outputGamma ? 1.0f : 0.0f;
    vkCmdPushConstants(commandList, pipeline->layout(),
        VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(push), &push);
    commandList.draw(3);
}

bool ForwardRenderer::render(CommandList& commandList,
    const ForwardView& view, std::span<const ForwardDraw> draws,
    std::string& errorMessage)
{
    errorMessage.clear();
    if (!ready() || draws.size() > m_layout.capacity) {
        errorMessage = "ForwardRenderer is not ready or draw capacity is insufficient";
        return false;
    }
    if (view.environment == nullptr || !view.environment->ready()) {
        errorMessage = "ForwardRenderer requires a ready environment descriptor";
        return false;
    }
    for (const ForwardDraw& draw : draws) {
        if (draw.model == nullptr || !draw.model->ready()
            || draw.materials == nullptr || !draw.materials->ready()) {
            errorMessage = "Forward draw borrows an invalid GPU asset";
            return false;
        }
        if (m_program->pipeline(pipelineKeyFor(draw.material)) == nullptr) {
            errorMessage = "Forward draw pipeline key was not prepared at the safe point";
            return false;
        }
    }
    if (!writeFrame(view) || !writeDraws(
            draws, view.outputGamma, view.environmentExposure)) {
        errorMessage = "Forward uniform buffer mapping failed";
        return false;
    }
    drawSkybox(commandList, view);
    std::vector<ForwardSortKey> sortKeys;
    sortKeys.reserve(draws.size());
    for (size_t index = 0; index < draws.size(); ++index) {
        const glm::vec3 delta = draws[index].boundsCenterWorld - view.cameraPosition;
        sortKeys.push_back({draws[index].material.alphaMode,
            glm::dot(delta, delta), index});
    }
    for (const size_t index : buildForwardDrawOrder(sortKeys)) {
        const ForwardDraw& draw = draws[index];
        const GpuMesh& mesh = draw.model->mesh();
        if (draw.subMeshIndex >= mesh.subMeshes().size()) {
            errorMessage = "Forward draw submesh index is out of range";
            return false;
        }
        const auto& subMesh = mesh.subMeshes()[draw.subMeshIndex];
        if (subMesh.indexCount == 0) continue;
        RHIPipeline* pipeline = m_program->pipeline(pipelineKeyFor(draw.material));
        pipeline->bind(commandList);
        const auto& material = draw.materials->material(draw.materialIndex);
        const VkDescriptorSet environmentSet = view.environment->descriptorSet();
        const std::array<VkDescriptorSet, forward_set::count> sets{
            material.descriptorSet, m_frameSet, environmentSet, m_drawSet};
        const uint64_t offset64 = index * m_layout.stride;
        if (offset64 > std::numeric_limits<uint32_t>::max()) {
            errorMessage = "Forward draw dynamic offset exceeds uint32_t";
            return false;
        }
        const uint32_t dynamicOffset = static_cast<uint32_t>(offset64);
        vkCmdBindDescriptorSets(commandList, VK_PIPELINE_BIND_POINT_GRAPHICS,
            pipeline->layout(), 0, static_cast<uint32_t>(sets.size()),
            sets.data(), 1, &dynamicOffset);
        const VkBuffer vertexBuffer = mesh.vertexBuffer().buffer();
        constexpr VkDeviceSize vertexOffset = 0;
        vkCmdBindVertexBuffers(commandList, 0, 1, &vertexBuffer, &vertexOffset);
        vkCmdBindIndexBuffer(commandList, mesh.indexBuffer().buffer(), 0,
            VK_INDEX_TYPE_UINT32);
        commandList.drawIndexed(subMesh.indexCount, 1, subMesh.indexStart);
    }
    return true;
}

bool ForwardRenderer::ready() const
{
    return m_device != nullptr && m_program != nullptr && m_program->ready()
        && m_frameBuffer != nullptr && m_drawBuffer != nullptr
        && m_framePool != VK_NULL_HANDLE && m_drawPool != VK_NULL_HANDLE
        && m_frameSet != VK_NULL_HANDLE && m_drawSet != VK_NULL_HANDLE;
}

void ForwardRenderer::reset()
{
    destroyDescriptors();
    m_drawBuffer.reset();
    m_frameBuffer.reset();
    m_layout = {};
    m_program = nullptr;
    m_device = nullptr;
    m_deviceHandle = VK_NULL_HANDLE;
}

void ForwardRenderer::destroyDescriptors()
{
    if (m_deviceHandle != VK_NULL_HANDLE) {
        if (m_drawPool != VK_NULL_HANDLE)
            vkDestroyDescriptorPool(m_deviceHandle, m_drawPool, nullptr);
        if (m_framePool != VK_NULL_HANDLE)
            vkDestroyDescriptorPool(m_deviceHandle, m_framePool, nullptr);
    }
    m_drawPool = VK_NULL_HANDLE;
    m_framePool = VK_NULL_HANDLE;
    m_drawSet = VK_NULL_HANDLE;
    m_frameSet = VK_NULL_HANDLE;
}

} // namespace ku
