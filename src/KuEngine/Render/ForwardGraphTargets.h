#pragma once

#include "RenderContext.h"

#include <array>
#include <cstdint>
#include <string_view>

#include <KuEngine/Core/ViewerLayout.h>

namespace ku {

class RHIDevice;

namespace forward_graph_resource {

inline constexpr std::string_view sceneColor = "ForwardSceneColor";
inline constexpr std::string_view sceneDepth = "ForwardSceneDepth";

} // namespace forward_graph_resource

struct ForwardGraphTargets {
    ImageDesc color{};
    ImageDesc depth{};
};

struct ForwardDisplayUvTransform {
    std::array<float, 2> scale{1.0f, 1.0f};
    std::array<float, 2> offset{0.0f, 0.0f};

    auto operator<=>(const ForwardDisplayUvTransform&) const = default;
};

[[nodiscard]] ForwardGraphTargets makeForwardGraphTargets(
    const RenderContext& context);
[[nodiscard]] ForwardGraphTargets makeForwardGraphTargets(
    VkFormat colorFormat,
    VkFormat depthFormat,
    VkClearColorValue clearColor,
    VkClearDepthStencilValue clearDepthStencil);
void validateForwardGraphTargetFormats(
    const RHIDevice& device,
    VkFormat colorFormat,
    VkFormat depthFormat);
[[nodiscard]] ForwardDisplayUvTransform calculateForwardDisplayUvTransform(
    const ViewerPixelRect& sceneRect,
    VkExtent2D sourceExtent) noexcept;
[[nodiscard]] bool forwardDisplaySourceChanged(
    uint64_t boundGeneration,
    VkImageView boundView,
    uint64_t resolvedGeneration,
    VkImageView resolvedView) noexcept;

} // namespace ku
