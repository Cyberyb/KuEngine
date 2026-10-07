#include "ForwardProgram.h"

#include <KuEngine/Asset/Model.h>
#include <KuEngine/RHI/RHIDevice.h>
#include <KuEngine/RHI/RHIPipeline.h>
#include <KuEngine/RHI/RHIShader.h>
#include <KuEngine/Render/PBRResources.h>

#include <array>
#include <cstddef>
#include <filesystem>
#include <stdexcept>
#include <vector>

namespace ku {

ForwardProgram::ForwardProgram() = default;

ForwardProgram::~ForwardProgram()
{
    reset();
}

bool ForwardProgram::initialize(
    RHIDevice& device,
    VkFormat colorFormat,
    VkFormat depthFormat,
    VkCompareOp depthCompareOp,
    std::string& errorMessage)
{
    reset();
    errorMessage.clear();
    m_device = &device;
    m_deviceHandle = device.device();
    m_colorFormat = colorFormat;
    m_depthFormat = depthFormat;
    m_depthCompareOp = depthCompareOp;
    try {
        const std::filesystem::path directory =
            std::filesystem::current_path() / "shaders" / "forward";
        m_vertexShader = std::make_unique<RHIShader>(
            device, ShaderDesc{directory / "forward.vert.spv",
                VK_SHADER_STAGE_VERTEX_BIT, "main"});
        m_pbrShader = std::make_unique<RHIShader>(
            device, ShaderDesc{directory / "pbr.frag.spv",
                VK_SHADER_STAGE_FRAGMENT_BIT, "main"});
        m_unlitShader = std::make_unique<RHIShader>(
            device, ShaderDesc{directory / "unlit.frag.spv",
                VK_SHADER_STAGE_FRAGMENT_BIT, "main"});
        m_skyboxVertexShader = std::make_unique<RHIShader>(
            device, ShaderDesc{directory / "skybox.vert.spv",
                VK_SHADER_STAGE_VERTEX_BIT, "main"});
        m_skyboxFragmentShader = std::make_unique<RHIShader>(
            device, ShaderDesc{directory / "skybox.frag.spv",
                VK_SHADER_STAGE_FRAGMENT_BIT, "main"});
        createLayouts();

        GraphicsPipelineDesc skybox{};
        skybox.shaders = {*m_skyboxVertexShader, *m_skyboxFragmentShader};
        skybox.colorFormats = {m_colorFormat};
        skybox.descriptorSetLayouts.assign(
            std::begin(m_setLayouts), std::end(m_setLayouts));
        skybox.pushConstantRanges = {VkPushConstantRange{
            VK_SHADER_STAGE_FRAGMENT_BIT,
            0,
            sizeof(ForwardSkyboxPushConstants)}};
        skybox.cullMode = VK_CULL_MODE_NONE;
        skybox.depthTest = false;
        skybox.depthWrite = false;
        skybox.depthFormat = m_depthFormat;
        m_skyboxPipeline = std::make_unique<RHIPipeline>(device, skybox);
    } catch (const std::exception& error) {
        errorMessage = error.what();
        reset();
        return false;
    } catch (...) {
        errorMessage = "Unknown ForwardProgram initialization failure";
        reset();
        return false;
    }
    if (!ready()) {
        errorMessage = "ForwardProgram did not reach the ready state";
        reset();
        return false;
    }
    return true;
}

void ForwardProgram::createLayouts()
{
    std::array<VkDescriptorSetLayoutBinding, pbr_material_binding::count>
        materialBindings{};
    for (uint32_t index = 0; index < materialBindings.size(); ++index) {
        materialBindings[index] = VkDescriptorSetLayoutBinding{
            index,
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            1,
            VK_SHADER_STAGE_FRAGMENT_BIT,
            nullptr};
    }
    VkDescriptorSetLayoutCreateInfo materialInfo{
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    materialInfo.bindingCount = static_cast<uint32_t>(materialBindings.size());
    materialInfo.pBindings = materialBindings.data();
    VK_CHECK(vkCreateDescriptorSetLayout(
        m_deviceHandle, &materialInfo, nullptr,
        &m_setLayouts[forward_set::material]));

    const auto createUniformLayout = [&](uint32_t set, VkDescriptorType type) {
        VkDescriptorSetLayoutBinding binding{};
        binding.binding = 0;
        binding.descriptorType = type;
        binding.descriptorCount = 1;
        binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT
            | VK_SHADER_STAGE_FRAGMENT_BIT;
        VkDescriptorSetLayoutCreateInfo info{
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
        info.bindingCount = 1;
        info.pBindings = &binding;
        VK_CHECK(vkCreateDescriptorSetLayout(
            m_deviceHandle, &info, nullptr, &m_setLayouts[set]));
    };
    createUniformLayout(forward_set::frame, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);

    VkDescriptorSetLayoutBinding environmentBinding{};
    environmentBinding.binding = pbr_environment_binding::environment;
    environmentBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    environmentBinding.descriptorCount = 1;
    environmentBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    VkDescriptorSetLayoutCreateInfo environmentInfo{
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    environmentInfo.bindingCount = 1;
    environmentInfo.pBindings = &environmentBinding;
    VK_CHECK(vkCreateDescriptorSetLayout(
        m_deviceHandle, &environmentInfo, nullptr,
        &m_setLayouts[forward_set::environment]));
    createUniformLayout(
        forward_set::draw, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC);
}

std::unique_ptr<RHIPipeline> ForwardProgram::createPipeline(
    const ForwardPipelineKey& key) const
{
    VkVertexInputBindingDescription vertexBinding{};
    vertexBinding.binding = 0;
    vertexBinding.stride = sizeof(asset::MeshVertex);
    vertexBinding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    const std::array<VkVertexInputAttributeDescription, 5> attributes{
        VkVertexInputAttributeDescription{0, 0, VK_FORMAT_R32G32B32_SFLOAT,
            static_cast<uint32_t>(offsetof(asset::MeshVertex, position))},
        VkVertexInputAttributeDescription{1, 0, VK_FORMAT_R32G32B32_SFLOAT,
            static_cast<uint32_t>(offsetof(asset::MeshVertex, normal))},
        VkVertexInputAttributeDescription{2, 0, VK_FORMAT_R32G32_SFLOAT,
            static_cast<uint32_t>(offsetof(asset::MeshVertex, uv0))},
        VkVertexInputAttributeDescription{3, 0, VK_FORMAT_R32G32_SFLOAT,
            static_cast<uint32_t>(offsetof(asset::MeshVertex, uv1))},
        VkVertexInputAttributeDescription{4, 0, VK_FORMAT_R32G32B32A32_SFLOAT,
            static_cast<uint32_t>(offsetof(asset::MeshVertex, tangent))},
    };
    GraphicsPipelineDesc desc{};
    desc.shaders = {*m_vertexShader,
        key.shadingModel == asset::ShadingModel::Unlit
            ? *m_unlitShader : *m_pbrShader};
    desc.colorFormats = {m_colorFormat};
    desc.vertexBindings = {vertexBinding};
    desc.vertexAttributes.assign(attributes.begin(), attributes.end());
    desc.descriptorSetLayouts.assign(
        std::begin(m_setLayouts), std::end(m_setLayouts));
    desc.cullMode = key.doubleSided ? VK_CULL_MODE_NONE : VK_CULL_MODE_BACK_BIT;
    desc.depthTest = true;
    desc.depthWrite = key.alphaMode != asset::AlphaMode::Blend;
    desc.blendEnable = key.alphaMode == asset::AlphaMode::Blend;
    desc.depthFormat = m_depthFormat;
    desc.depthCompareOp = m_depthCompareOp;
    return std::make_unique<RHIPipeline>(*m_device, desc);
}

bool ForwardProgram::ensurePipelines(
    std::span<const ForwardPipelineKey> keys,
    std::string& errorMessage)
{
    errorMessage.clear();
    try {
        std::map<ForwardPipelineKey, std::unique_ptr<RHIPipeline>> candidates;
        for (const ForwardPipelineKey& key : keys) {
            const bool validShading =
                key.shadingModel == asset::ShadingModel::PBR
                || key.shadingModel == asset::ShadingModel::Unlit;
            const bool validAlpha = key.alphaMode == asset::AlphaMode::Opaque
                || key.alphaMode == asset::AlphaMode::Mask
                || key.alphaMode == asset::AlphaMode::Blend;
            if (!validShading || !validAlpha) {
                throw std::invalid_argument("Invalid forward pipeline key");
            }
            if (m_pipelines.contains(key) || candidates.contains(key)) {
                continue;
            }
            candidates.emplace(key, createPipeline(key));
        }
        m_pipelines.merge(candidates);
    } catch (const std::exception& error) {
        errorMessage = error.what();
        return false;
    } catch (...) {
        errorMessage = "Unknown forward pipeline creation failure";
        return false;
    }
    return true;
}

RHIPipeline* ForwardProgram::pipeline(const ForwardPipelineKey& key) const
{
    const auto found = m_pipelines.find(key);
    return found == m_pipelines.end() ? nullptr : found->second.get();
}

VkDescriptorSetLayout ForwardProgram::setLayout(uint32_t set) const
{
    return set < forward_set::count ? m_setLayouts[set] : VK_NULL_HANDLE;
}

bool ForwardProgram::ready() const
{
    if (m_device == nullptr || m_deviceHandle == VK_NULL_HANDLE
        || m_skyboxPipeline == nullptr) {
        return false;
    }
    for (VkDescriptorSetLayout layout : m_setLayouts) {
        if (layout == VK_NULL_HANDLE) {
            return false;
        }
    }
    return true;
}

void ForwardProgram::reset()
{
    m_pipelines.clear();
    m_skyboxPipeline.reset();
    m_vertexShader.reset();
    m_pbrShader.reset();
    m_unlitShader.reset();
    m_skyboxVertexShader.reset();
    m_skyboxFragmentShader.reset();
    destroyLayouts();
    m_device = nullptr;
    m_colorFormat = VK_FORMAT_UNDEFINED;
    m_depthFormat = VK_FORMAT_UNDEFINED;
}

void ForwardProgram::destroyLayouts()
{
    if (m_deviceHandle != VK_NULL_HANDLE) {
        for (VkDescriptorSetLayout& layout : m_setLayouts) {
            if (layout != VK_NULL_HANDLE) {
                vkDestroyDescriptorSetLayout(m_deviceHandle, layout, nullptr);
                layout = VK_NULL_HANDLE;
            }
        }
    }
    m_deviceHandle = VK_NULL_HANDLE;
}

} // namespace ku
