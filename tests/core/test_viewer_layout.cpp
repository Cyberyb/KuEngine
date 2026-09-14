#include <gtest/gtest.h>

#include <KuEngine/Core/SceneInteraction.h>
#include <KuEngine/Core/ViewerLayout.h>

#include <cmath>
#include <limits>

namespace {

ku::ViewerLayout makeInteractionLayout()
{
    return ku::calculateViewerLayout({
        .windowLogicalWidth = 1280.0,
        .windowLogicalHeight = 720.0,
        .framebufferExtent = {1280, 720},
        .sidebarLogicalWidth = 380.0,
        .reserveSidebar = true,
    });
}

} // namespace

TEST(ViewerLayoutTest, ExpandedReservesSidebarAndCollapsedRestoresFullScene)
{
    const ku::ViewerLayout expanded = ku::calculateViewerLayout({
        .windowLogicalWidth = 1280.0,
        .windowLogicalHeight = 720.0,
        .framebufferExtent = {1280, 720},
        .sidebarLogicalWidth = 384.0,
        .reserveSidebar = true,
    });
    EXPECT_TRUE(expanded.sidebarReserved);
    EXPECT_FALSE(expanded.sidebarOverlayFallback);
    EXPECT_DOUBLE_EQ(expanded.sceneLogical.width, 896.0);
    EXPECT_EQ(expanded.sceneFramebuffer.width, 896u);
    EXPECT_EQ(expanded.sceneFramebuffer.height, 720u);
    EXPECT_FLOAT_EQ(expanded.sceneAspect, 896.0f / 720.0f);

    const ku::ViewerLayout collapsed = ku::calculateViewerLayout({
        .windowLogicalWidth = 1280.0,
        .windowLogicalHeight = 720.0,
        .framebufferExtent = {1280, 720},
        .sidebarLogicalWidth = 0.0,
        .reserveSidebar = false,
    });
    EXPECT_FALSE(collapsed.sidebarReserved);
    EXPECT_DOUBLE_EQ(collapsed.sceneLogical.width, 1280.0);
    EXPECT_EQ(collapsed.sceneFramebuffer.width, 1280u);
}

TEST(ViewerLayoutTest, MapsIndependentDpiScalesAndOddBoundariesWithoutOverflow)
{
    const ku::ViewerLayout scaled = ku::calculateViewerLayout({
        .windowLogicalWidth = 1000.0,
        .windowLogicalHeight = 500.0,
        .framebufferExtent = {2000, 750},
        .sidebarLogicalWidth = 300.0,
        .reserveSidebar = true,
    });
    EXPECT_DOUBLE_EQ(scaled.framebufferScaleX, 2.0);
    EXPECT_DOUBLE_EQ(scaled.framebufferScaleY, 1.5);
    EXPECT_EQ(scaled.sceneFramebuffer.width, 1400u);
    EXPECT_EQ(scaled.sceneFramebuffer.height, 750u);

    const ku::ViewerLayout odd = ku::calculateViewerLayout({
        .windowLogicalWidth = 1001.0,
        .windowLogicalHeight = 777.0,
        .framebufferExtent = {1501, 901},
        .sidebarLogicalWidth = 333.0,
        .reserveSidebar = true,
    });
    EXPECT_GT(odd.sceneFramebuffer.width, 0u);
    EXPECT_LE(
        odd.sceneFramebuffer.x + odd.sceneFramebuffer.width,
        odd.framebufferExtent.width);
    EXPECT_LE(
        odd.sceneFramebuffer.y + odd.sceneFramebuffer.height,
        odd.framebufferExtent.height);
}

TEST(ViewerLayoutTest, UsesFullscreenOverlayFallbackForTinyOrInvalidLogicalSize)
{
    const ku::ViewerLayout tiny = ku::calculateViewerLayout({
        .windowLogicalWidth = 50.0,
        .windowLogicalHeight = 40.0,
        .framebufferExtent = {100, 80},
        .sidebarLogicalWidth = 50.0,
        .reserveSidebar = true,
    });
    EXPECT_TRUE(tiny.sidebarOverlayFallback);
    EXPECT_FALSE(tiny.sidebarReserved);
    EXPECT_DOUBLE_EQ(tiny.sceneLogical.width, 50.0);
    EXPECT_EQ(tiny.sceneFramebuffer.width, 100u);
    EXPECT_TRUE(tiny.sceneRenderable());

    const ku::ViewerLayout invalid = ku::calculateViewerLayout({
        .windowLogicalWidth = std::numeric_limits<double>::quiet_NaN(),
        .windowLogicalHeight = 0.0,
        .framebufferExtent = {640, 480},
        .sidebarLogicalWidth = std::numeric_limits<double>::infinity(),
        .reserveSidebar = true,
    });
    EXPECT_FALSE(invalid.sceneInputVisible());
    EXPECT_TRUE(invalid.sceneRenderable());
    EXPECT_EQ(invalid.sceneFramebuffer.width, 640u);
    EXPECT_EQ(invalid.sceneFramebuffer.height, 480u);
    EXPECT_TRUE(std::isfinite(invalid.sceneAspect));

    const ku::ViewerLayout zeroFramebuffer = ku::calculateViewerLayout({
        .windowLogicalWidth = 800.0,
        .windowLogicalHeight = 600.0,
    });
    EXPECT_FALSE(zeroFramebuffer.sceneRenderable());
}

TEST(ViewerLayoutTest, LogicalHitTestingUsesHalfOpenRightAndBottomEdges)
{
    const ku::ViewerLogicalRect scene{10.0, 20.0, 100.0, 50.0};
    EXPECT_TRUE(scene.contains(10.0, 20.0));
    EXPECT_TRUE(scene.contains(109.999, 69.999));
    EXPECT_FALSE(scene.contains(110.0, 30.0));
    EXPECT_FALSE(scene.contains(20.0, 70.0));
    EXPECT_FALSE(scene.contains(std::numeric_limits<double>::quiet_NaN(), 30.0));
}

TEST(ViewerLayoutTest, ClampsUiOverlayWithoutReservingCompactScene)
{
    const ku::ViewerLayout compact = ku::calculateViewerLayout({
        .windowLogicalWidth = 800.0,
        .windowLogicalHeight = 600.0,
        .framebufferExtent = {1600, 900},
        .uiOverlayLogical = {650.0, 0.0, 270.0, 210.0},
    });

    EXPECT_DOUBLE_EQ(compact.sceneLogical.width, 800.0);
    EXPECT_EQ(compact.sceneFramebuffer.width, 1600u);
    EXPECT_DOUBLE_EQ(compact.inputExclusionLogical.x, 650.0);
    EXPECT_DOUBLE_EQ(compact.inputExclusionLogical.width, 150.0);
    EXPECT_DOUBLE_EQ(compact.inputExclusionLogical.height, 210.0);
}

TEST(ViewerLayoutTest, FitsViewportInsideAttachmentAndNeverReturnsZero)
{
    const ku::ViewerPixelRect fitted = ku::fitViewerRectToExtent(
        {999, 999, 0, 500},
        {320, 200});
    EXPECT_EQ(fitted.x, 319u);
    EXPECT_EQ(fitted.y, 199u);
    EXPECT_EQ(fitted.width, 1u);
    EXPECT_EQ(fitted.height, 1u);
    EXPECT_LE(fitted.x + fitted.width, 320u);
    EXPECT_LE(fitted.y + fitted.height, 200u);
}

TEST(SceneInteractionGateTest, ScenePressLatchesAndContinuesOutsideUntilRelease)
{
    const ku::ViewerLayout layout = makeInteractionLayout();
    ku::SceneInteractionGate gate;

    const ku::ScenePointerAction pressed = gate.update(layout, {
        .logicalX = 100.0,
        .logicalY = 100.0,
        .primaryDown = true,
        .primaryPressed = true,
    });
    EXPECT_FALSE(pressed.rotate);
    EXPECT_TRUE(gate.dragging());

    const ku::ScenePointerAction outside = gate.update(layout, {
        .logicalX = 1100.0,
        .logicalY = 100.0,
        .deltaX = 7.0,
        .deltaY = -4.0,
        .primaryDown = true,
        .uiCapturesPointer = true,
    });
    EXPECT_TRUE(outside.rotate);
    EXPECT_DOUBLE_EQ(outside.rotationDeltaX, 7.0);
    EXPECT_DOUBLE_EQ(outside.rotationDeltaY, -4.0);

    EXPECT_FALSE(gate.update(layout, {}).rotate);
    EXPECT_FALSE(gate.dragging());
}

TEST(SceneInteractionGateTest, UiOrSidebarPressCannotStartAfterEnteringScene)
{
    const ku::ViewerLayout layout = makeInteractionLayout();
    ku::SceneInteractionGate gate;

    (void)gate.update(layout, {
        .logicalX = 1000.0,
        .logicalY = 100.0,
        .primaryDown = true,
        .primaryPressed = true,
        .uiCapturesPointer = true,
    });
    const ku::ScenePointerAction movedInside = gate.update(layout, {
        .logicalX = 100.0,
        .logicalY = 100.0,
        .deltaX = 5.0,
        .primaryDown = true,
    });
    EXPECT_FALSE(movedInside.rotate);
    EXPECT_FALSE(gate.dragging());
}

TEST(SceneInteractionGateTest, FocusLossOrMinimizeClearsDragLatch)
{
    const ku::ViewerLayout layout = makeInteractionLayout();
    ku::SceneInteractionGate gate;
    (void)gate.update(layout, {
        .logicalX = 100.0,
        .logicalY = 100.0,
        .primaryDown = true,
        .primaryPressed = true,
    });
    EXPECT_TRUE(gate.dragging());

    (void)gate.update(layout, {
        .logicalX = 100.0,
        .logicalY = 100.0,
        .primaryDown = true,
        .windowActive = false,
    });
    EXPECT_FALSE(gate.dragging());
    EXPECT_FALSE(gate.update(layout, {
        .logicalX = 100.0,
        .logicalY = 100.0,
        .deltaX = 5.0,
        .primaryDown = true,
    }).rotate);
}

TEST(SceneInteractionGateTest, ScrollRequiresActiveUncapturedPointerInsideScene)
{
    const ku::ViewerLayout layout = makeInteractionLayout();
    ku::SceneInteractionGate gate;

    EXPECT_DOUBLE_EQ(gate.update(layout, {
        .logicalX = 100.0,
        .logicalY = 100.0,
        .scrollY = 2.0,
    }).scrollY, 2.0);
    EXPECT_DOUBLE_EQ(gate.update(layout, {
        .logicalX = layout.sceneLogical.width,
        .logicalY = 100.0,
        .scrollY = 2.0,
    }).scrollY, 0.0);
    EXPECT_DOUBLE_EQ(gate.update(layout, {
        .logicalX = 100.0,
        .logicalY = 100.0,
        .scrollY = 2.0,
        .uiCapturesPointer = true,
    }).scrollY, 0.0);
    EXPECT_DOUBLE_EQ(gate.update(layout, {
        .logicalX = 100.0,
        .logicalY = 100.0,
        .scrollY = 2.0,
        .windowActive = false,
    }).scrollY, 0.0);
}

TEST(SceneInteractionGateTest, OverlayOccupancyBlocksCompactUiWithoutImGuiHistory)
{
    const ku::ViewerLayout layout = ku::calculateViewerLayout({
        .windowLogicalWidth = 1280.0,
        .windowLogicalHeight = 720.0,
        .framebufferExtent = {1280, 720},
        .uiOverlayLogical = {1010.0, 0.0, 270.0, 210.0},
    });
    ku::SceneInteractionGate gate;

    (void)gate.update(layout, {
        .logicalX = 1100.0,
        .logicalY = 100.0,
        .primaryDown = true,
        .primaryPressed = true,
    });
    EXPECT_FALSE(gate.dragging());
    EXPECT_DOUBLE_EQ(gate.update(layout, {
        .logicalX = 1100.0,
        .logicalY = 100.0,
        .scrollY = 2.0,
    }).scrollY, 0.0);
}

TEST(SceneInteractionGateTest, FocusLossEpochClearsUnobservedDragAndRequiresNewPress)
{
    const ku::ViewerLayout layout = makeInteractionLayout();
    ku::SceneInteractionGate gate;

    (void)gate.update(layout, {
        .logicalX = 100.0,
        .logicalY = 100.0,
        .primaryDown = true,
        .primaryPressed = true,
        .interactionEpoch = 20,
    });
    EXPECT_TRUE(gate.dragging());
    EXPECT_TRUE(gate.update(layout, {
        .logicalX = 1100.0,
        .logicalY = 100.0,
        .deltaX = 4.0,
        .primaryDown = true,
        .interactionEpoch = 20,
    }).rotate);

    // No gate.update occurs during the inactive/focus-lost frames. The next
    // active sample carries Input's changed continuity epoch.
    EXPECT_FALSE(gate.update(layout, {
        .logicalX = 100.0,
        .logicalY = 100.0,
        .deltaX = 9.0,
        .primaryDown = true,
        .interactionEpoch = 21,
    }).rotate);
    EXPECT_FALSE(gate.dragging());

    // Continued physical hold remains blocked, even if a bad producer were
    // to claim a press edge. Release is the only way to re-arm the gate.
    EXPECT_FALSE(gate.update(layout, {
        .logicalX = 100.0,
        .logicalY = 100.0,
        .primaryDown = true,
        .primaryPressed = true,
        .interactionEpoch = 21,
    }).rotate);
    (void)gate.update(layout, {
        .logicalX = 100.0,
        .logicalY = 100.0,
        .interactionEpoch = 21,
    });
    (void)gate.update(layout, {
        .logicalX = 100.0,
        .logicalY = 100.0,
        .primaryDown = true,
        .primaryPressed = true,
        .interactionEpoch = 21,
    });
    EXPECT_TRUE(gate.dragging());
}

TEST(SceneInteractionGateTest, MinimizeRestoreEpochClearsDragAcrossSkippedResizeFrames)
{
    const ku::ViewerLayout layout = makeInteractionLayout();
    ku::SceneInteractionGate gate;

    (void)gate.update(layout, {
        .logicalX = 100.0,
        .logicalY = 100.0,
        .primaryDown = true,
        .primaryPressed = true,
        .interactionEpoch = 30,
    });
    EXPECT_TRUE(gate.dragging());

    // Engine may skip Pass update both while minimized and during the first
    // swapchain-recreation frame. The epoch still invalidates the old latch.
    EXPECT_FALSE(gate.update(layout, {
        .logicalX = 100.0,
        .logicalY = 100.0,
        .deltaX = 8.0,
        .primaryDown = true,
        .interactionEpoch = 32,
    }).rotate);
    EXPECT_FALSE(gate.dragging());

    (void)gate.update(layout, {
        .logicalX = 100.0,
        .logicalY = 100.0,
        .interactionEpoch = 32,
    });
    (void)gate.update(layout, {
        .logicalX = 100.0,
        .logicalY = 100.0,
        .primaryDown = true,
        .primaryPressed = true,
        .interactionEpoch = 32,
    });
    EXPECT_TRUE(gate.dragging());
}
