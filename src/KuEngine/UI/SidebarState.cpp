#include "SidebarState.h"

#include <algorithm>
#include <cmath>

namespace ku {
namespace {

float sanitizeDimension(float value) noexcept
{
    return std::isfinite(value) ? std::max(value, 0.0f) : 0.0f;
}

} // namespace

SidebarSections SidebarState::sections(bool showStats) const noexcept
{
    if (m_expanded) {
        return SidebarSections{
            .performance = showStats,
            .parameters = true,
            .renderGraph = true,
            .reopenControl = false,
        };
    }

    return SidebarSections{
        .performance = showStats && m_compactStatsVisible,
        .parameters = false,
        .renderGraph = false,
        .reopenControl = true,
    };
}

SidebarLayoutMode SidebarState::layoutMode(bool showStats) const noexcept
{
    if (m_expanded) {
        return SidebarLayoutMode::Expanded;
    }
    return showStats && m_compactStatsVisible
        ? SidebarLayoutMode::Compact
        : SidebarLayoutMode::ReopenOnly;
}

SidebarLayout SidebarLayoutPolicy::calculate(
    float displayWidth,
    float displayHeight,
    SidebarLayoutMode mode) const noexcept
{
    const float safeWidth = sanitizeDimension(displayWidth);
    const float safeHeight = sanitizeDimension(displayHeight);

    float desiredWidth = 0.0f;
    float desiredHeight = safeHeight;
    switch (mode) {
        case SidebarLayoutMode::Expanded:
            desiredWidth = std::clamp(safeWidth * 0.30f, 280.0f, 440.0f);
            break;
        case SidebarLayoutMode::Compact:
            desiredWidth = 270.0f;
            desiredHeight = std::min(safeHeight, 210.0f);
            break;
        case SidebarLayoutMode::ReopenOnly:
        default:
            desiredWidth = 180.0f;
            desiredHeight = std::min(safeHeight, 54.0f);
            break;
    }

    const float width = std::min(desiredWidth, safeWidth);
    return SidebarLayout{
        .x = std::max(0.0f, safeWidth - width),
        .y = 0.0f,
        .width = width,
        .height = desiredHeight,
    };
}

SidebarFrameLayout describeSidebarFrame(
    const SidebarState& state,
    const SidebarLayoutPolicy& policy,
    float logicalWindowWidth,
    float logicalWindowHeight,
    bool showStats) noexcept
{
    const SidebarLayoutMode mode = state.layoutMode(showStats);
    const SidebarLayout layout = policy.calculate(
        logicalWindowWidth,
        logicalWindowHeight,
        mode);
    return SidebarFrameLayout{
        .mode = mode,
        .sections = state.sections(showStats),
        .window = layout,
        .reservedSceneLogicalWidth =
            mode == SidebarLayoutMode::Expanded ? layout.width : 0.0f,
    };
}

} // namespace ku
