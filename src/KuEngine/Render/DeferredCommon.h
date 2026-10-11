#pragma once

#include <KuEngine/Render/ForwardCommon.h>
#include <KuEngine/Render/RenderContext.h>
#include <array>
#include <optional>

namespace ku {
// Stored exactly in positionModel.a. Zero is the CLEAR / no geometry sentinel.
enum class GBufferShadingCode : uint32_t { Empty = 0, PBR = 1, Unlit = 2 };
inline constexpr size_t gbufferAttachmentCount = 4;
inline constexpr VkFormat gbufferFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
inline constexpr std::array<std::string_view, gbufferAttachmentCount> gbufferNames{
    "GBufferBaseMetallic", "GBufferNormalRoughness",
    "GBufferPositionModel", "GBufferEmissiveAo"};
struct GBufferTargets {
    // Location / binding order is part of the public shader contract.
    std::array<ImageDesc, gbufferAttachmentCount> colors{};
    ImageDesc depth{};
};
[[nodiscard]] std::optional<GBufferShadingCode> encodeGBufferShadingModel(
    asset::ShadingModel model) noexcept;
[[nodiscard]] bool supportsDeferredGeometry(const ForwardPipelineKey& key) noexcept;
[[nodiscard]] GBufferTargets makeGBufferTargets(
    VkFormat depthFormat, VkClearDepthStencilValue clearDepth);
void validateGBufferCapabilities(const VkPhysicalDeviceLimits& limits,
    VkFormatFeatureFlags colorFeatures);
void validateGBufferFormats(const RHIDevice& device, VkFormat depthFormat);
inline constexpr std::string_view deferredSceneColorName = "ForwardSceneColor";
[[nodiscard]] ImageDesc makeDeferredSceneColor(const RenderContext& context);
} // namespace ku
