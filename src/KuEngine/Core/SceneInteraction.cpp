#include "SceneInteraction.h"

#include <cmath>

namespace ku {
namespace {

double finiteOrZero(double value) noexcept
{
    return std::isfinite(value) ? value : 0.0;
}

} // namespace

ScenePointerAction SceneInteractionGate::update(
    const ViewerLayout& layout,
    const ScenePointerSample& sample) noexcept
{
    ScenePointerAction action{};
    if (!m_epochInitialized) {
        m_epochInitialized = true;
        m_interactionEpoch = sample.interactionEpoch;
    } else if (sample.interactionEpoch != m_interactionEpoch) {
        m_interactionEpoch = sample.interactionEpoch;
        m_dragging = false;
        // A button held across an input discontinuity cannot manufacture a
        // new drag. It must be released before a later press can latch.
        // Input suppresses press edges on the resume sample. If a later
        // resize-early-return hides the released state from this gate, a real
        // new press edge is nevertheless safe to accept.
        m_waitForPrimaryRelease =
            sample.primaryDown && !sample.primaryPressed;
    }

    if (!sample.windowActive) {
        m_dragging = false;
        m_waitForPrimaryRelease = m_waitForPrimaryRelease || sample.primaryDown;
        return action;
    }

    if (m_waitForPrimaryRelease) {
        if (!sample.primaryDown) {
            m_waitForPrimaryRelease = false;
        }
        return action;
    }

    const bool pointerInsideScene = layout.sceneLogical.contains(
        sample.logicalX,
        sample.logicalY)
        && !layout.inputExclusionLogical.contains(
            sample.logicalX,
            sample.logicalY);
    if (!sample.uiCapturesPointer && pointerInsideScene) {
        action.scrollY = finiteOrZero(sample.scrollY);
    }

    if (!sample.primaryDown) {
        m_dragging = false;
        return action;
    }

    if (sample.primaryPressed) {
        m_dragging = pointerInsideScene && !sample.uiCapturesPointer;
        return action;
    }

    if (m_dragging) {
        action.rotate = true;
        action.rotationDeltaX = finiteOrZero(sample.deltaX);
        action.rotationDeltaY = finiteOrZero(sample.deltaY);
    }
    return action;
}

} // namespace ku
