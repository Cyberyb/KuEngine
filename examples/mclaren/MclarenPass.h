#pragma once

#include <array>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <glm/vec3.hpp>

#include <KuEngine/Asset/AssetConfig.h>
#include <KuEngine/Asset/Model.h>
#include <KuEngine/RHI/CommandList.h>
#include <KuEngine/RHI/RHIBuffer.h>
#include <KuEngine/RHI/RHIDevice.h>
#include <KuEngine/RHI/RHIPipeline.h>
#include <KuEngine/RHI/ResourceUploader.h>
#include <KuEngine/RHI/RHIShader.h>
#include <KuEngine/RHI/RHITexture.h>
#include <KuEngine/Render/GpuMesh.h>
#include <KuEngine/Render/PBRCommon.h>
#include <KuEngine/Render/PBRRenderer.h>
#include <KuEngine/Render/RenderPass.h>
#include <KuEngine/Render/TextureFactory.h>

namespace ku {

class MclarenPass : public RenderPass {
public:
    MclarenPass();
    ~MclarenPass() override;

    [[nodiscard]] std::string_view name() const override { return "Mclaren"; }

    void initialize(RHIDevice& device) override;
    void setup(RenderGraphBuilder& builder) override;
    void execute(CommandList& cmd, const FrameData& frame) override;
    void drawUI() override;
    bool supportsInlineUI() const override { return true; }
    void drawUIInline() override;
    void onResize(uint32_t width, uint32_t height) override;

    void addRotation(float deltaYaw, float deltaPitch);
    void setDepthFormat(VkFormat depthFormat) { m_depthFormat = depthFormat; }

private:
    using PushConstants = PBRPushConstants;
    using FrameUniforms = PBRFrameUniforms;
    using SkyboxPushConstants = PBRSkyboxPushConstants;
    using MaterialBinding = PBRMaterialBinding;

    std::filesystem::path resolveScenePath() const;
    std::filesystem::path resolveModelPath() const;
    std::filesystem::path resolveEnvironmentPath() const;
    void loadSceneAndMaterialConfig();
    void loadMaterialConfig(const std::filesystem::path& materialPath);
    bool createAndUploadTexture(
        const asset::TextureData& textureData,
        VkFormat format,
        std::unique_ptr<RHITexture>& outTexture);
    bool createSolidColorTexture(
        std::array<uint8_t, 4> rgba,
        VkFormat format,
        std::unique_ptr<RHITexture>& outTexture);
    bool createAndUploadHdrTexture(
        const std::filesystem::path& hdrPath,
        std::unique_ptr<RHITexture>& outTexture);
    void destroyDescriptorResources();

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
    std::vector<MaterialBinding> m_materialBindings;
    std::vector<std::unique_ptr<RHITexture>> m_materialTextures;
    std::unique_ptr<RHITexture> m_fallbackWhiteTexture;
    std::unique_ptr<RHITexture> m_fallbackNormalTexture;
    std::unique_ptr<RHITexture> m_fallbackOrmTexture;
    std::unique_ptr<RHITexture> m_fallbackEmissiveTexture;
    std::unique_ptr<RHITexture> m_environmentTexture;

    std::unique_ptr<RHIBuffer> m_frameUniformBuffer;

    VkDescriptorSetLayout m_descriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool m_descriptorPool = VK_NULL_HANDLE;
    VkSampler m_sampler = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_frameDescriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool m_frameDescriptorPool = VK_NULL_HANDLE;
    VkDescriptorSet m_frameDescriptorSet = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_environmentDescriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool m_environmentDescriptorPool = VK_NULL_HANDLE;
    VkDescriptorSet m_environmentDescriptorSet = VK_NULL_HANDLE;
    VkDevice m_deviceHandle = VK_NULL_HANDLE;

    std::filesystem::path m_modelPathOverride;
    std::vector<std::filesystem::path> m_sceneModelPaths;
    std::vector<std::filesystem::path> m_sceneMaterialPaths;

    std::string m_scenePathString;
    std::string m_materialPathString;
    std::string m_modelPathString;
    std::string m_environmentPathString;
    std::string m_loadError;
    asset::MaterialConfig m_materialConfig{};

    bool m_sceneConfigUsed = false;
    bool m_materialConfigUsed = false;

    float m_aspect = 16.0f / 9.0f;
    float m_offX = 0.0f;
    float m_visibleWidth = 1.0f;
    uint32_t m_viewportWidth = 1280;
    uint32_t m_viewportHeight = 720;
    float m_distance = 4.0f;
    float m_yaw = 0.0f;
    float m_pitch = 0.0f;
    glm::vec3 m_cameraPosition{0.0f, 0.0f, 4.0f};
    glm::vec3 m_cameraTarget{0.0f, 0.0f, 0.0f};
    glm::vec3 m_cameraUp{0.0f, 1.0f, 0.0f};
    float m_cameraFovYDegrees = 60.0f;
    float m_cameraNear = 0.1f;
    float m_cameraFar = 200.0f;

    glm::vec3 m_lightDirection{0.35f, 1.0f, 0.45f};
    glm::vec3 m_lightColor{1.0f, 1.0f, 1.0f};
    float m_lightIntensity = 1.0f;

    float m_fitScale = 1.0f;
    glm::vec3 m_modelCenter{0.0f, 0.0f, 0.0f};
    VkFormat m_depthFormat = VK_FORMAT_D32_SFLOAT;
    bool m_enableTextureSampling = true;
    bool m_enableNormalMap = true;
    bool m_enableOrmMap = true;
    bool m_enableEnvironmentMap = true;
    bool m_enableSkybox = true;
    bool m_flipUvY = true;
    bool m_enableOutputGamma = true;
    float m_environmentIntensity = 0.7f;
    float m_environmentExposure = 1.0f;

    std::array<float, 4> m_globalBaseColorFactor = {1.0f, 1.0f, 1.0f, 1.0f};
};

} // namespace ku
