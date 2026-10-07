#include "ForwardCommon.h"

#include <KuEngine/Render/PBRCommon.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <stdexcept>

namespace ku {
namespace {

asset::AlphaMode parseAlphaMode(const std::string& value)
{
    const std::string lower = toLower(value);
    if (lower == "mask") {
        return asset::AlphaMode::Mask;
    }
    if (lower == "blend") {
        return asset::AlphaMode::Blend;
    }
    return asset::AlphaMode::Opaque;
}

void applyUvSet(
    asset::MaterialData::TextureTransform& transform,
    const asset::MaterialConfig::TextureBindingConfig& config)
{
    if (config.hasUvSet) {
        transform.texCoord = config.uvSet <= 0 ? 0u : 1u;
    }
}

MaterialTextureVariantKey textureVariantKey(
    const asset::MaterialConfig::TextureBindingConfig& binding)
{
    MaterialTextureVariantKey key{};
    key.hasSource = binding.hasSource;
    key.hasColorSpace = binding.hasColorSpace;
    key.hasChannelMapping = binding.hasChannelMapping;
    if (key.hasSource) key.source = toLower(binding.source);
    if (key.hasColorSpace) key.colorSpace = toLower(binding.colorSpace);
    if (key.hasChannelMapping) {
        key.channelMapping = toLower(binding.channelMapping);
    }
    return key;
}

} // namespace

ResolvedMaterial resolveMaterial(
    const asset::MaterialData& base,
    const asset::MaterialConfig* overrides)
{
    ResolvedMaterial resolved{};
    resolved.shadingModel = base.shadingModel;
    resolved.alphaMode = base.alphaMode;
    resolved.doubleSided = base.doubleSided;
    resolved.alphaCutoff = base.alphaCutoff;
    resolved.baseColorFactor = {
        base.baseColorFactor.r,
        base.baseColorFactor.g,
        base.baseColorFactor.b,
        base.baseColorFactor.a,
    };
    resolved.emissiveFactor = {
        base.emissiveFactor.r,
        base.emissiveFactor.g,
        base.emissiveFactor.b,
    };
    resolved.metallicFactor = base.metallicFactor;
    resolved.roughnessFactor = base.roughnessFactor;
    resolved.normalScale = base.normalScale;
    resolved.occlusionStrength = base.occlusionStrength;
    resolved.textureTransforms = {
        base.baseColorTransform,
        base.normalTransform,
        base.metallicRoughnessTransform,
        base.occlusionTransform,
        base.emissiveTransform,
    };

    if (overrides == nullptr) {
        return resolved;
    }
    if (overrides->hasShadingModel) {
        resolved.shadingModel = overrides->shadingModel;
    }
    if (overrides->hasAlphaMode) {
        resolved.alphaMode = parseAlphaMode(overrides->alphaMode);
    }
    if (overrides->hasDoubleSided) {
        resolved.doubleSided = overrides->doubleSided;
    }
    if (overrides->hasAlphaCutoff) {
        resolved.alphaCutoff = std::clamp(overrides->alphaCutoff, 0.0f, 1.0f);
    }
    if (overrides->hasBaseColorFactor) {
        resolved.baseColorFactor = overrides->baseColorFactor;
    }
    if (overrides->hasEmissiveFactor) {
        resolved.emissiveFactor = overrides->emissiveFactor;
    }
    if (overrides->hasMetallicFactor) {
        resolved.metallicFactor = overrides->metallicFactor;
    }
    if (overrides->hasRoughnessFactor) {
        resolved.roughnessFactor = overrides->roughnessFactor;
    }
    if (overrides->hasNormalScale) {
        resolved.normalScale = overrides->normalScale;
    }
    if (overrides->hasOcclusionStrength) {
        resolved.occlusionStrength = overrides->occlusionStrength;
    }
    applyUvSet(resolved.textureTransforms[0], overrides->baseColorBinding);
    applyUvSet(resolved.textureTransforms[1], overrides->normalBinding);
    applyUvSet(resolved.textureTransforms[2],
        overrides->metallicRoughnessBinding);
    applyUvSet(resolved.textureTransforms[3], overrides->occlusionBinding);
    if (isCombinedOrmBinding(overrides->ormBinding)) {
        if (!overrides->metallicRoughnessBinding.hasUvSet) {
            applyUvSet(resolved.textureTransforms[2], overrides->ormBinding);
        }
        if (!overrides->occlusionBinding.hasUvSet) {
            applyUvSet(resolved.textureTransforms[3], overrides->ormBinding);
        }
    }
    applyUvSet(resolved.textureTransforms[4], overrides->emissiveBinding);
    return resolved;
}

MaterialGpuVariantKey materialGpuVariantKey(
    const asset::SceneInstance& instance)
{
    MaterialGpuVariantKey key{};
    key.mesh = instance.mesh;
    if (!instance.hasMaterialOverride) return key;
    const asset::MaterialConfig& config = instance.materialOverride;
    key.textureBindings = {
        textureVariantKey(config.baseColorBinding),
        textureVariantKey(config.normalBinding),
        textureVariantKey(config.metallicRoughnessBinding),
        textureVariantKey(config.occlusionBinding),
        textureVariantKey(config.emissiveBinding),
        textureVariantKey(config.ormBinding),
    };
    return key;
}

MaterialGpuVariantPlan buildMaterialGpuVariantPlan(
    std::span<const asset::SceneInstance> instances)
{
    std::map<MaterialGpuVariantKey, size_t> representatives;
    for (size_t index = 0; index < instances.size(); ++index) {
        representatives.try_emplace(
            materialGpuVariantKey(instances[index]), index);
    }
    if (representatives.size() > std::numeric_limits<uint32_t>::max()) {
        throw std::overflow_error("Material GPU variant count exceeds uint32_t");
    }

    MaterialGpuVariantPlan plan{};
    plan.variants.reserve(representatives.size());
    plan.representativeInstanceIndices.reserve(representatives.size());
    std::map<MaterialGpuVariantKey, uint32_t> variantIndices;
    for (const auto& [key, representative] : representatives) {
        const uint32_t variantIndex =
            static_cast<uint32_t>(plan.variants.size());
        plan.variants.push_back(key);
        plan.representativeInstanceIndices.push_back(representative);
        variantIndices.emplace(key, variantIndex);
    }

    plan.instanceVariantIndices.reserve(instances.size());
    for (const asset::SceneInstance& instance : instances) {
        plan.instanceVariantIndices.push_back(
            variantIndices.at(materialGpuVariantKey(instance)));
    }
    return plan;
}

ForwardPipelineKey pipelineKeyFor(
    const ResolvedMaterial& material) noexcept
{
    return ForwardPipelineKey{
        material.shadingModel,
        material.alphaMode,
        material.doubleSided,
    };
}

std::vector<size_t> buildForwardDrawOrder(
    std::span<const ForwardSortKey> keys)
{
    std::vector<size_t> order;
    order.reserve(keys.size());
    for (const ForwardSortKey& key : keys) {
        if (key.alphaMode != asset::AlphaMode::Blend) {
            order.push_back(key.originalIndex);
        }
    }
    std::vector<ForwardSortKey> blended;
    blended.reserve(keys.size());
    for (const ForwardSortKey& key : keys) {
        if (key.alphaMode == asset::AlphaMode::Blend) {
            blended.push_back(key);
        }
    }
    std::stable_sort(
        blended.begin(),
        blended.end(),
        [](const ForwardSortKey& lhs, const ForwardSortKey& rhs) {
            return lhs.distanceSquared > rhs.distanceSquared;
        });
    for (const ForwardSortKey& key : blended) {
        order.push_back(key.originalIndex);
    }
    return order;
}

bool calculateForwardDynamicBufferLayout(
    size_t drawCapacity,
    VkDeviceSize elementSize,
    VkDeviceSize minimumAlignment,
    ForwardDynamicBufferLayout& outLayout) noexcept
{
    ForwardDynamicBufferLayout candidate{};
    if (drawCapacity == 0
        || drawCapacity > std::numeric_limits<uint32_t>::max()) {
        return false;
    }
    try {
        candidate.stride = alignedUniformBufferStride(
            elementSize,
            minimumAlignment);
    } catch (...) {
        return false;
    }
    if (candidate.stride == 0
        || candidate.stride > std::numeric_limits<uint32_t>::max()
        || drawCapacity
            > std::numeric_limits<VkDeviceSize>::max() / candidate.stride) {
        return false;
    }
    candidate.totalSize = candidate.stride * drawCapacity;
    const VkDeviceSize lastOffset = candidate.stride * (drawCapacity - 1);
    if (lastOffset > std::numeric_limits<uint32_t>::max()
        || elementSize > candidate.totalSize - lastOffset) {
        return false;
    }
    candidate.capacity = static_cast<uint32_t>(drawCapacity);
    outLayout = candidate;
    return true;
}

} // namespace ku
