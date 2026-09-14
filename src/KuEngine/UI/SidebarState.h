// KuEngine sidebar state and layout policy. This file intentionally has no ImGui/Vulkan dependency.
#pragma once

namespace ku {

enum class SidebarLayoutMode {
    Expanded,
    Compact,
    ReopenOnly,
};

struct SidebarSections {
    bool performance = false;
    bool parameters = false;
    bool renderGraph = false;
    bool reopenControl = false;
};

class SidebarState {
public:
    [[nodiscard]] bool expanded() const noexcept { return m_expanded; }
    [[nodiscard]] bool compactStatsVisible() const noexcept
    {
        return m_compactStatsVisible;
    }

    void expand() noexcept { m_expanded = true; }
    void collapse() noexcept { m_expanded = false; }
    void toggleExpanded() noexcept { m_expanded = !m_expanded; }
    void setCompactStatsVisible(bool visible) noexcept
    {
        m_compactStatsVisible = visible;
    }

    [[nodiscard]] SidebarSections sections(bool showStats) const noexcept;
    [[nodiscard]] SidebarLayoutMode layoutMode(bool showStats) const noexcept;

private:
    bool m_expanded = true;
    bool m_compactStatsVisible = true;
};

struct SidebarLayout {
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
};

class SidebarLayoutPolicy {
public:
    [[nodiscard]] SidebarLayout calculate(
        float displayWidth,
        float displayHeight,
        SidebarLayoutMode mode) const noexcept;
};

struct SidebarFrameLayout {
    SidebarLayoutMode mode = SidebarLayoutMode::Expanded;
    SidebarSections sections{};
    SidebarLayout window{};
    float reservedSceneLogicalWidth = 0.0f;
};

// Captures all sidebar decisions used by one frame. Later state mutations
// affect only the next call, keeping UI, rendering, and input in agreement.
[[nodiscard]] SidebarFrameLayout describeSidebarFrame(
    const SidebarState& state,
    const SidebarLayoutPolicy& policy,
    float logicalWindowWidth,
    float logicalWindowHeight,
    bool showStats) noexcept;

} // namespace ku
