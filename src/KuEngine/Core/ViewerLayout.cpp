#include "ViewerLayout.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace ku {
namespace {

double sanitizeDimension(double value) noexcept
{
    return std::isfinite(value) ? std::max(value, 0.0) : 0.0;
}

ViewerLogicalRect clampLogicalRect(
    const ViewerLogicalRect& requested,
    double windowWidth,
    double windowHeight) noexcept
{
    const double x = std::clamp(
        std::isfinite(requested.x) ? requested.x : 0.0,
        0.0,
        windowWidth);
    const double y = std::clamp(
        std::isfinite(requested.y) ? requested.y : 0.0,
        0.0,
        windowHeight);
    const double width = std::min(
        sanitizeDimension(requested.width),
        windowWidth - x);
    const double height = std::min(
        sanitizeDimension(requested.height),
        windowHeight - y);
    return {x, y, width, height};
}

uint32_t mapLogicalRightToPixels(
    double logicalRight,
    double scale,
    uint32_t framebufferWidth) noexcept
{
    if (framebufferWidth == 0) {
        return 0;
    }
    if (!std::isfinite(logicalRight)
        || !std::isfinite(scale)
        || logicalRight <= 0.0
        || scale <= 0.0) {
        return framebufferWidth;
    }

    const double mapped = std::floor(logicalRight * scale);
    if (!std::isfinite(mapped) || mapped >= framebufferWidth) {
        return framebufferWidth;
    }
    return std::max(1u, static_cast<uint32_t>(mapped));
}

} // namespace

bool ViewerLogicalRect::contains(
    double logicalX,
    double logicalY) const noexcept
{
    if (!std::isfinite(logicalX)
        || !std::isfinite(logicalY)
        || !std::isfinite(x)
        || !std::isfinite(y)
        || !std::isfinite(width)
        || !std::isfinite(height)
        || width <= 0.0
        || height <= 0.0) {
        return false;
    }

    return logicalX >= x
        && logicalY >= y
        && logicalX < x + width
        && logicalY < y + height;
}

ViewerLayout calculateViewerLayout(const ViewerLayoutInput& input) noexcept
{
    ViewerLayout layout{};
    const double logicalWidth = sanitizeDimension(input.windowLogicalWidth);
    const double logicalHeight = sanitizeDimension(input.windowLogicalHeight);
    const double sidebarWidth = sanitizeDimension(input.sidebarLogicalWidth);
    const double minimumSceneWidth =
        sanitizeDimension(input.minimumSceneLogicalWidth);

    layout.windowLogical = {0.0, 0.0, logicalWidth, logicalHeight};
    layout.sceneLogical = layout.windowLogical;
    layout.inputExclusionLogical = clampLogicalRect(
        input.uiOverlayLogical,
        logicalWidth,
        logicalHeight);
    layout.framebufferExtent = input.framebufferExtent;

    if (input.reserveSidebar && sidebarWidth > 0.0 && logicalWidth > 0.0) {
        const double clampedSidebar = std::min(sidebarWidth, logicalWidth);
        const double candidateSceneWidth = logicalWidth - clampedSidebar;
        if (candidateSceneWidth >= minimumSceneWidth
            && candidateSceneWidth > 0.0) {
            layout.sceneLogical.width = candidateSceneWidth;
            layout.sidebarReserved = true;
        } else {
            // Tiny-window policy: preserve a valid full scene and let the sidebar
            // overlay it instead of emitting a zero-sized Vulkan viewport.
            layout.sidebarOverlayFallback = true;
        }
    }

    if (logicalWidth > 0.0) {
        layout.framebufferScaleX =
            static_cast<double>(input.framebufferExtent.width) / logicalWidth;
    }
    if (logicalHeight > 0.0) {
        layout.framebufferScaleY =
            static_cast<double>(input.framebufferExtent.height) / logicalHeight;
    }

    if (input.framebufferExtent.width > 0
        && input.framebufferExtent.height > 0) {
        uint32_t scenePixelWidth = input.framebufferExtent.width;
        if (logicalWidth > 0.0 && layout.sceneLogical.width < logicalWidth) {
            scenePixelWidth = mapLogicalRightToPixels(
                layout.sceneLogical.x + layout.sceneLogical.width,
                layout.framebufferScaleX,
                input.framebufferExtent.width);
        }

        layout.sceneFramebuffer = {
            0,
            0,
            std::clamp(scenePixelWidth, 1u, input.framebufferExtent.width),
            input.framebufferExtent.height,
        };
        layout.sceneAspect = static_cast<float>(
            static_cast<double>(layout.sceneFramebuffer.width)
            / static_cast<double>(layout.sceneFramebuffer.height));
        if (!std::isfinite(layout.sceneAspect) || layout.sceneAspect <= 0.0f) {
            layout.sceneAspect = 1.0f;
        }
    }

    return layout;
}

ViewerPixelRect fitViewerRectToExtent(
    const ViewerPixelRect& requested,
    ViewerPixelExtent attachmentExtent) noexcept
{
    if (attachmentExtent.width == 0 || attachmentExtent.height == 0) {
        return {};
    }

    const uint32_t x = std::min(requested.x, attachmentExtent.width - 1);
    const uint32_t y = std::min(requested.y, attachmentExtent.height - 1);
    const uint32_t availableWidth = attachmentExtent.width - x;
    const uint32_t availableHeight = attachmentExtent.height - y;
    return ViewerPixelRect{
        x,
        y,
        std::clamp(requested.width, 1u, availableWidth),
        std::clamp(requested.height, 1u, availableHeight),
    };
}

} // namespace ku
