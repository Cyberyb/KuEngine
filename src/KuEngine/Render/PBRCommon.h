// KuEngine PBR 公共模块：集中定义 PBR 着色所需的 GPU 数据结构、材质绑定与辅助接口。
#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <KuEngine/Asset/AssetConfig.h>
#include <KuEngine/Asset/Model.h>
#include <vulkan/vulkan.h>

namespace ku {

struct alignas(16) PBRPushConstants {
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

struct alignas(16) PBRFrameUniforms {
    float model[16];
    float cameraPos[4];
    float invViewProj[16];
    float lightDirIntensity[4];
    float emissiveFactor[4];
    float alphaParams[4];
};

struct alignas(16) PBRSkyboxPushConstants {
    float params[4];
};

struct PBRMaterialBinding {
    std::array<float, 4> baseColorFactor{1.0f, 1.0f, 1.0f, 1.0f};
    std::array<float, 3> emissiveFactor{0.0f, 0.0f, 0.0f};
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
    float emissiveUvRotation = 0.0f;
    float baseTexCoord = 0.0f;
    float normalTexCoord = 0.0f;
    float ormTexCoord = 0.0f;
    float emissiveTexCoord = 0.0f;

    bool hasBaseColorTexture = false;
    bool hasNormalTexture = false;
    bool hasOrmTexture = false;
    bool hasEmissiveTexture = false;
    VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
};

std::string toLower(std::string_view text);
bool isDisabledSource(const std::string& sourceLower);
float clampTexCoordSet(uint32_t texCoord);

const asset::TextureData* resolveGltfTexture(
    std::string_view source,
    const asset::MaterialData& material);

VkFormat formatForBinding(
    const asset::MaterialConfig::TextureBindingConfig& binding,
    VkFormat defaultFormat,
    std::string_view bindingName);

float alphaModeToFlag(const asset::MaterialConfig& config);

} // namespace ku
