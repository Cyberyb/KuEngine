// Mclaren GPU 资源容器：统一管理上传、材质/环境描述符、管线和 PBR 绘制资源生命周期。
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include <vulkan/vulkan.h>

#include <KuEngine/Render/PBRCommon.h>

namespace ku {

class CommandList;
class GpuMesh;
class PBRRenderer;
class RHIBuffer;
class RHIDevice;
class RHIPipeline;
class RHIShader;
class RHITexture;
class ResourceUploader;
class TextureFactory;
class MclarenSceneAsset;

struct MclarenResourceStats {
    uint32_t texturedBase = 0;
    uint32_t texturedNormal = 0;
    uint32_t texturedOrm = 0;
    uint32_t texturedEmissive = 0;
};

class MclarenRenderResources {
public:
    MclarenRenderResources();
    ~MclarenRenderResources();

    MclarenRenderResources(const MclarenRenderResources&) = delete;
    MclarenRenderResources& operator=(const MclarenRenderResources&) = delete;

    [[nodiscard]] bool initialize(
        RHIDevice& device,
        const MclarenSceneAsset& scene,
        VkFormat colorFormat,
        VkFormat depthFormat,
        VkCompareOp depthCompareOp,
        std::string& errorMessage);
    void reset();

    [[nodiscard]] bool ready() const;
    [[nodiscard]] GpuMesh& mesh() const;
    [[nodiscard]] const std::vector<PBRMaterialBinding>& materials() const;
    [[nodiscard]] PBRRenderer& pbrRenderer() const;
    [[nodiscard]] const MclarenResourceStats& stats() const { return m_stats; }

    [[nodiscard]] bool writeSkyboxFrame(const PBRFrameUniforms& frame);
    void drawSkybox(
        CommandList& cmd,
        float exposure,
        bool encodeOutputGamma) const;

private:
    [[nodiscard]] bool createAndUploadTexture(
        const asset::TextureData& textureData,
        VkFormat format,
        std::unique_ptr<RHITexture>& outTexture);
    [[nodiscard]] bool createSolidColorTexture(
        std::array<uint8_t, 4> rgba,
        VkFormat format,
        std::unique_ptr<RHITexture>& outTexture);
    [[nodiscard]] bool createAndUploadHdrTexture(
        const std::filesystem::path& hdrPath,
        std::unique_ptr<RHITexture>& outTexture);

    void createDescriptorLayouts(RHIDevice& device);
    void createFrameResources(
        RHIDevice& device,
        size_t drawCount);
    void createMaterialResources(
        const MclarenSceneAsset& scene);
    void createEnvironmentResources(
        const MclarenSceneAsset& scene);
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
    std::unique_ptr<ResourceUploader> m_resourceUploader;
    std::unique_ptr<TextureFactory> m_textureFactory;
    std::unique_ptr<GpuMesh> m_gpuMesh;
    std::unique_ptr<PBRRenderer> m_pbrRenderer;

    std::vector<PBRMaterialBinding> m_materialBindings;
    std::vector<std::unique_ptr<RHITexture>> m_materialTextures;
    std::unique_ptr<RHITexture> m_fallbackWhiteTexture;
    std::unique_ptr<RHITexture> m_fallbackNormalTexture;
    std::unique_ptr<RHITexture> m_fallbackOrmTexture;
    std::unique_ptr<RHITexture> m_fallbackEmissiveTexture;
    std::unique_ptr<RHITexture> m_environmentTexture;

    std::unique_ptr<RHIBuffer> m_frameUniformBuffer;
    uint32_t m_frameUniformStride = 0;

    VkDescriptorSetLayout m_materialDescriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool m_materialDescriptorPool = VK_NULL_HANDLE;
    VkSampler m_sampler = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_frameDescriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool m_frameDescriptorPool = VK_NULL_HANDLE;
    VkDescriptorSet m_frameDescriptorSet = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_environmentDescriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool m_environmentDescriptorPool = VK_NULL_HANDLE;
    VkDescriptorSet m_environmentDescriptorSet = VK_NULL_HANDLE;
    VkDevice m_deviceHandle = VK_NULL_HANDLE;

    MclarenResourceStats m_stats{};
};

} // namespace ku
