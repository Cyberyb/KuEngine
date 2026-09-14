// Pure scene-pointer gating shared by orbit-style camera controls.
#pragma once

#include "ViewerLayout.h"

#include <cstdint>

namespace ku {

struct ScenePointerSample {
    double logicalX = 0.0;
    double logicalY = 0.0;
    double deltaX = 0.0;
    double deltaY = 0.0;
    double scrollY = 0.0;
    bool primaryDown = false;
    bool primaryPressed = false;
    bool windowActive = true;
    bool uiCapturesPointer = false;
    // A changed epoch means one or more frames lost input continuity, even if
    // the gate was not called during those frames.
    uint64_t interactionEpoch = 0;
};

struct ScenePointerAction {
    double rotationDeltaX = 0.0;
    double rotationDeltaY = 0.0;
    double scrollY = 0.0;
    bool rotate = false;
};

class SceneInteractionGate {
public:
    [[nodiscard]] ScenePointerAction update(
        const ViewerLayout& layout,
        const ScenePointerSample& sample) noexcept;

    void reset() noexcept
    {
        m_dragging = false;
        m_waitForPrimaryRelease = false;
        m_epochInitialized = false;
    }
    [[nodiscard]] bool dragging() const noexcept { return m_dragging; }

private:
    bool m_dragging = false;
    bool m_waitForPrimaryRelease = false;
    bool m_epochInitialized = false;
    uint64_t m_interactionEpoch = 0;
};

} // namespace ku
