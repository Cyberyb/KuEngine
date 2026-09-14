// Viewer geometry shared by runtime, render passes, and input without UI/Vulkan dependencies.
#pragma once

#include <cstdint>

namespace ku {

struct ViewerLogicalRect {
    double x = 0.0;
    double y = 0.0;
    double width = 0.0;
    double height = 0.0;

    // Rectangles use half-open bounds: [x, x + width) x [y, y + height).
    [[nodiscard]] bool contains(double logicalX, double logicalY) const noexcept;
};

struct ViewerPixelExtent {
    uint32_t width = 0;
    uint32_t height = 0;
};

struct ViewerPixelRect {
    uint32_t x = 0;
    uint32_t y = 0;
    uint32_t width = 0;
    uint32_t height = 0;
};

struct ViewerLayoutInput {
    double windowLogicalWidth = 0.0;
    double windowLogicalHeight = 0.0;
    ViewerPixelExtent framebufferExtent{};
    double sidebarLogicalWidth = 0.0;
    bool reserveSidebar = false;
    double minimumSceneLogicalWidth = 64.0;
    // Logical UI occupancy for deterministic pointer exclusion. This is
    // separate from scene reservation because compact UI overlays the scene.
    ViewerLogicalRect uiOverlayLogical{};
};

struct ViewerLayout {
    ViewerLogicalRect windowLogical{};
    ViewerLogicalRect sceneLogical{};
    ViewerLogicalRect inputExclusionLogical{};
    ViewerPixelExtent framebufferExtent{};
    ViewerPixelRect sceneFramebuffer{};
    double framebufferScaleX = 0.0;
    double framebufferScaleY = 0.0;
    float sceneAspect = 1.0f;
    bool sidebarReserved = false;
    bool sidebarOverlayFallback = false;

    [[nodiscard]] bool sceneInputVisible() const noexcept
    {
        return sceneLogical.width > 0.0 && sceneLogical.height > 0.0;
    }

    [[nodiscard]] bool sceneRenderable() const noexcept
    {
        return sceneFramebuffer.width > 0 && sceneFramebuffer.height > 0;
    }
};

[[nodiscard]] ViewerLayout calculateViewerLayout(
    const ViewerLayoutInput& input) noexcept;

// Fits a requested scene rectangle to a non-zero attachment. Invalid or empty
// requests fall back to a one-pixel rectangle inside that attachment.
[[nodiscard]] ViewerPixelRect fitViewerRectToExtent(
    const ViewerPixelRect& requested,
    ViewerPixelExtent attachmentExtent) noexcept;

} // namespace ku
