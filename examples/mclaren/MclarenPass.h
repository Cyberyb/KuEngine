#pragma once

#include <array>
#include <memory>
#include <string>
#include <string_view>

#include <glm/vec3.hpp>

#include <KuEngine/Asset/Model.h>
#include <KuEngine/RHI/CommandList.h>
#include <KuEngine/RHI/RHIBuffer.h>
#include <KuEngine/RHI/RHIDevice.h>
#include <KuEngine/RHI/RHIPipeline.h>
#include <KuEngine/RHI/RHIShader.h>
#include <KuEngine/RHI/RHITexture.h>
#include <KuEngine/Render/RenderPass.h>

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
    void onResize(uint32_t width, uint32_t height) override;

    void addRotation(float deltaYaw, float deltaPitch);
    void setDepthFormat(VkFormat depthFormat) { m_depthFormat = depthFormat; }

private:
    struct alignas(16) PushConstants {
        float mvp[16];
        float normalRows[12];
        float baseColorFactor[4];
        float materialParams[4];
        float materialFactors[4];
        float baseUvScaleOffset[4];
        float normalUvScaleOffset[4];
        float ormUvScaleOffset[4];
        float uvTransformParams0[4];
        float uvTransformParams1[4];
    };

    struct MaterialBinding {
        std::array<float, 4> baseColorFactor{1.0f, 1.0f, 1.0f, 1.0f};
        float metallicFactor = 1.0f;
        float roughnessFactor = 1.0f;
        float normalScale = 1.0f;
        float occlusionStrength = 1.0f;

        std::array<float, 4> baseUvScaleOffset{1.0f, 1.0f, 0.0f, 0.0f};
        std::array<float, 4> normalUvScaleOffset{1.0f, 1.0f, 0.0f, 0.0f};
        std::array<float, 4> ormUvScaleOffset{1.0f, 1.0f, 0.0f, 0.0f};
        float baseUvRotation = 0.0f;
        float normalUvRotation = 0.0f;
        float ormUvRotation = 0.0f;
        float baseTexCoord = 0.0f;
        float normalTexCoord = 0.0f;
        float ormTexCoord = 0.0f;

        bool hasBaseColorTexture = false;
        bool hasNormalTexture = false;
        bool hasOrmTexture = false;
        VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
    };

    std::filesystem::path resolveModelPath() const;
    bool createAndUploadTexture(
        RHIDevice& device,
        const asset::TextureData& textureData,
        VkFormat format,
        std::unique_ptr<RHITexture>& outTexture);
    bool createSolidColorTexture(
        RHIDevice& device,
        std::array<uint8_t, 4> rgba,
        VkFormat format,
        std::unique_ptr<RHITexture>& outTexture);
    void destroyDescriptorResources();

    std::unique_ptr<RHIShader> m_vertShader;
    std::unique_ptr<RHIShader> m_fragShader;
    std::unique_ptr<RHIPipeline> m_pipeline;
    std::vector<MaterialBinding> m_materialBindings;
    std::vector<std::unique_ptr<RHITexture>> m_materialTextures;
    std::vector<asset::SubMeshData> m_subMeshes;
    std::unique_ptr<RHITexture> m_fallbackWhiteTexture;
    std::unique_ptr<RHITexture> m_fallbackNormalTexture;
    std::unique_ptr<RHITexture> m_fallbackOrmTexture;

    std::unique_ptr<RHIBuffer> m_vertexBuffer;
    std::unique_ptr<RHIBuffer> m_indexBuffer;

    VkDescriptorSetLayout m_descriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool m_descriptorPool = VK_NULL_HANDLE;
    VkSampler m_sampler = VK_NULL_HANDLE;
    VkDevice m_deviceHandle = VK_NULL_HANDLE;

    std::string m_modelPathString;
    std::string m_loadError;

    uint32_t m_vertexCount = 0;
    uint32_t m_indexCount = 0;

    float m_aspect = 16.0f / 9.0f;
    float m_distance = 4.0f;
    float m_yaw = 0.0f;
    float m_pitch = 0.0f;
    float m_fitScale = 1.0f;
    glm::vec3 m_modelCenter{0.0f, 0.0f, 0.0f};
    VkFormat m_depthFormat = VK_FORMAT_D32_SFLOAT;
    bool m_enableTextureSampling = true;
    bool m_enableNormalMap = true;
    bool m_enableOrmMap = true;
    bool m_flipUvY = true;

    std::array<float, 4> m_globalBaseColorFactor = {1.0f, 1.0f, 1.0f, 1.0f};
};

} // namespace ku
