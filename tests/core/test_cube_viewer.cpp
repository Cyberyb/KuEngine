#include <gtest/gtest.h>

#include "CubePass.h"

#include <imgui.h>

#include <limits>

TEST(CubeViewerTest, UsesFiniteSceneViewportAspect)
{
    ImGui::CreateContext();
    ImGui::GetIO().IniFilename = nullptr;

    ku::CubePass cube;
    ku::FrameData frame{};
    frame.viewerLayout = ku::calculateViewerLayout({
        .windowLogicalWidth = 1280.0,
        .windowLogicalHeight = 720.0,
        .framebufferExtent = {1920, 1080},
        .sidebarLogicalWidth = 384.0,
        .reserveSidebar = true,
    });
    cube.update(frame);

    EXPECT_TRUE(std::isfinite(cube.projectionAspect()));
    EXPECT_FLOAT_EQ(cube.projectionAspect(), frame.viewerLayout.sceneAspect);
    ImGui::DestroyContext();
}

TEST(CubeViewerTest, ScrollUsesExplicitDirectionSensitivityAndUiRange)
{
    EXPECT_FLOAT_EQ(
        ku::adjustCubeCameraDistance(3.5f, 1.0),
        3.5f - ku::cubeCameraScrollStep);
    EXPECT_FLOAT_EQ(
        ku::adjustCubeCameraDistance(3.5f, -1.0),
        3.5f + ku::cubeCameraScrollStep);
    EXPECT_FLOAT_EQ(
        ku::adjustCubeCameraDistance(2.1f, 100.0),
        ku::cubeCameraMinimumDistance);
    EXPECT_FLOAT_EQ(
        ku::adjustCubeCameraDistance(7.9f, -100.0),
        ku::cubeCameraMaximumDistance);
    EXPECT_FLOAT_EQ(
        ku::adjustCubeCameraDistance(
            3.5f,
            std::numeric_limits<double>::quiet_NaN()),
        3.5f);
}

TEST(CubeViewerTest, GateApprovedScrollChangesDistanceWithoutAffectingAspect)
{
    ku::CubePass cube;
    const ku::ViewerLayout layout = ku::calculateViewerLayout({
        .windowLogicalWidth = 1280.0,
        .windowLogicalHeight = 720.0,
        .framebufferExtent = {1280, 720},
        .sidebarLogicalWidth = 384.0,
        .reserveSidebar = true,
    });
    ku::SceneInteractionGate gate;
    const float originalAspect = cube.projectionAspect();

    const ku::ScenePointerAction sceneScroll = gate.update(layout, {
        .logicalX = 100.0,
        .logicalY = 100.0,
        .scrollY = 1.0,
    });
    cube.applyViewerInteraction(sceneScroll);
    EXPECT_FLOAT_EQ(
        cube.cameraDistance(),
        ku::cubeCameraDefaultDistance - ku::cubeCameraScrollStep);
    EXPECT_FLOAT_EQ(cube.projectionAspect(), originalAspect);

    const float afterScroll = cube.cameraDistance();
    cube.applyViewerInteraction({
        .rotationDeltaX = 4.0,
        .rotationDeltaY = -2.0,
        .rotate = true,
    });
    EXPECT_FLOAT_EQ(cube.cameraDistance(), afterScroll);
    EXPECT_FLOAT_EQ(cube.projectionAspect(), originalAspect);
}

TEST(CubeViewerTest, CapturedOrSidebarScrollCannotChangeDistance)
{
    ku::CubePass cube;
    const ku::ViewerLayout expanded = ku::calculateViewerLayout({
        .windowLogicalWidth = 1280.0,
        .windowLogicalHeight = 720.0,
        .framebufferExtent = {1280, 720},
        .sidebarLogicalWidth = 384.0,
        .reserveSidebar = true,
        .uiOverlayLogical = {896.0, 0.0, 384.0, 720.0},
    });
    ku::SceneInteractionGate gate;
    EXPECT_FLOAT_EQ(cube.cameraDistance(), ku::cubeCameraDefaultDistance);

    cube.applyViewerInteraction(gate.update(expanded, {
        .logicalX = 100.0,
        .logicalY = 100.0,
        .scrollY = 2.0,
        .uiCapturesPointer = true,
    }));
    EXPECT_FLOAT_EQ(cube.cameraDistance(), ku::cubeCameraDefaultDistance);

    cube.applyViewerInteraction(gate.update(expanded, {
        .logicalX = 1000.0,
        .logicalY = 100.0,
        .scrollY = 2.0,
    }));
    EXPECT_FLOAT_EQ(cube.cameraDistance(), ku::cubeCameraDefaultDistance);

    const ku::ViewerLayout compact = ku::calculateViewerLayout({
        .windowLogicalWidth = 1280.0,
        .windowLogicalHeight = 720.0,
        .framebufferExtent = {1280, 720},
        .uiOverlayLogical = {1010.0, 0.0, 270.0, 210.0},
    });
    cube.applyViewerInteraction(gate.update(compact, {
        .logicalX = 1100.0,
        .logicalY = 100.0,
        .scrollY = 2.0,
    }));
    EXPECT_FLOAT_EQ(cube.cameraDistance(), ku::cubeCameraDefaultDistance);

    const ku::ViewerLayout reopenOnly = ku::calculateViewerLayout({
        .windowLogicalWidth = 1280.0,
        .windowLogicalHeight = 720.0,
        .framebufferExtent = {1280, 720},
        .uiOverlayLogical = {1100.0, 0.0, 180.0, 54.0},
    });
    cube.applyViewerInteraction(gate.update(reopenOnly, {
        .logicalX = 1150.0,
        .logicalY = 20.0,
        .scrollY = -2.0,
    }));
    EXPECT_FLOAT_EQ(cube.cameraDistance(), ku::cubeCameraDefaultDistance);
}
