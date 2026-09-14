// Mclaren GPU 装配：只拥有着色器、管线、帧 UBO 和 Renderer，借用公共 GPU 资产。
#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

#include <vulkan/vulkan.h>

#include <KuEngine/Render/PBRCommon.h>

namespace ku {

class CommandList;
class GpuModelAsset;
class PBRMaterialResources;
class PBREnvironmentResources;
class PBRRenderer;
class RHIBuffer;
class RHIDevice;
class RHIPipeline;
class RHIShader;
class MclarenSceneAsset;

class MclarenRenderResources {
public:
    MclarenRenderResources();
    ~MclarenRenderResources();

    MclarenRenderResources(const MclarenRenderResources&) = delete;
    MclarenRenderResources& operator=(const MclarenRenderResources&) = delete;

    [[nodiscard]] bool initialize(
        RHIDevice& device,
        const MclarenSceneAsset& scene,
        GpuModelAsset& model,
        PBRMaterialResources& materials,
        PBREnvironmentResources& environment,
        VkFormat colorFormat,
        VkFormat depthFormat,
        VkCompareOp depthCompareOp,
        std::string& errorMessage);
    void reset();

    [[nodiscard]] bool ready() const;
    [[nodiscard]] PBRRenderer& pbrRenderer() const;

    [[nodiscard]] bool writeSkyboxFrame(const PBRFrameUniforms& frame);
    void drawSkybox(
        CommandList& cmd,
        float exposure,
        bool encodeOutputGamma) const;

private:
    void createFrameDescriptorLayout();
    void createFrameResources(
        RHIDevice& device,
        size_t drawCount);
    void createPipelines(
        RHIDevice& device,
        const MclarenSceneAsset& scene,
        VkFormat colorFormat,
        VkFormat depthFormat,
        VkCompareOp depthCompareOp);
    void configurePbrRenderer(VkFormat depthFormat);
    void destroyVulkanHandles();

    std::unique_ptr<RHIShader> m_vertShader;
    std::unique_ptr<RHIShader> m_fragShader;
    std::unique_ptr<RHIShader> m_skyboxVertShader;
    std::unique_ptr<RHIShader> m_skyboxFragShader;
    std::unique_ptr<RHIPipeline> m_pipeline;
    std::unique_ptr<RHIPipeline> m_skyboxPipeline;
    std::unique_ptr<PBRRenderer> m_pbrRenderer;

    std::unique_ptr<RHIBuffer> m_frameUniformBuffer;
    uint32_t m_frameUniformStride = 0;

    VkDescriptorSetLayout m_frameDescriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool m_frameDescriptorPool = VK_NULL_HANDLE;
    VkDescriptorSet m_frameDescriptorSet = VK_NULL_HANDLE;
    VkDevice m_deviceHandle = VK_NULL_HANDLE;

    // Borrowed. MclarenPass declares these owners before this assembly object.
    GpuModelAsset* m_model = nullptr;
    PBRMaterialResources* m_materials = nullptr;
    PBREnvironmentResources* m_environment = nullptr;
};

} // namespace ku
