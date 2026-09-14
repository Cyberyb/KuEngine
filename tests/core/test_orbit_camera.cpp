#include <gtest/gtest.h>

#include <cmath>
#include <limits>

#include "OrbitCameraController.h"

namespace {

ku::asset::SceneCameraConfig cameraConfig()
{
    ku::asset::SceneCameraConfig config{};
    config.position = {0.0f, 0.0f, 5.0f};
    config.target = {0.0f, 0.0f, 0.0f};
    config.up = {0.0f, 1.0f, 0.0f};
    config.fovYDeg = 60.0f;
    config.nearPlane = 0.1f;
    config.farPlane = 200.0f;
    return config;
}

ku::ViewerLayout viewerLayout()
{
    return ku::calculateViewerLayout({
        .windowLogicalWidth = 1280.0,
        .windowLogicalHeight = 720.0,
        .framebufferExtent = {1920, 1080},
        .sidebarLogicalWidth = 384.0,
        .reserveSidebar = true,
        .uiOverlayLogical = {896.0, 0.0, 384.0, 720.0},
    });
}

ku::CameraInputSample sample(uint64_t epoch = 1)
{
    return ku::CameraInputSample{
        .viewerLayout = viewerLayout(),
        .pointerX = 100.0,
        .pointerY = 100.0,
        .interactionEpoch = epoch,
    };
}

void armFreeInput(ku::OrbitCameraController& camera, uint64_t epoch = 1)
{
    camera.setMode(ku::ViewerCameraMode::FreeFly);
    camera.updateInput(sample(epoch));

    ku::CameraInputSample press = sample(epoch);
    press.rightDown = true;
    press.rightPressed = true;
    camera.updateInput(press);
    EXPECT_TRUE(camera.freeInputFocused());

    camera.updateInput(sample(epoch));
    EXPECT_TRUE(camera.freeInputFocused());
}

void expectVecNear(
    const glm::vec3& actual,
    const glm::vec3& expected,
    float tolerance = 1e-4f)
{
    EXPECT_NEAR(actual.x, expected.x, tolerance);
    EXPECT_NEAR(actual.y, expected.y, tolerance);
    EXPECT_NEAR(actual.z, expected.z, tolerance);
}

void expectMatrixNear(
    const glm::mat4& actual,
    const glm::mat4& expected,
    float tolerance = 1e-4f)
{
    for (int column = 0; column < 4; ++column) {
        for (int row = 0; row < 4; ++row) {
            EXPECT_NEAR(actual[column][row], expected[column][row], tolerance);
        }
    }
}

bool finiteMatrix(const glm::mat4& matrix)
{
    for (int column = 0; column < 4; ++column) {
        for (int row = 0; row < 4; ++row) {
            if (!std::isfinite(matrix[column][row])) {
                return false;
            }
        }
    }
    return true;
}

glm::vec3 runMovement(
    bool forward,
    bool backward,
    bool left,
    bool right,
    bool down,
    bool up,
    float deltaTime = 1.0f,
    float speed = 3.0f)
{
    ku::OrbitCameraController camera;
    camera.reset(cameraConfig());
    camera.setMoveSpeed(speed);
    armFreeInput(camera);
    const glm::vec3 initial = camera.freePosition();

    ku::CameraInputSample movement = sample();
    movement.deltaTime = deltaTime;
    movement.moveForward = forward;
    movement.moveBackward = backward;
    movement.moveLeft = left;
    movement.moveRight = right;
    movement.moveDown = down;
    movement.moveUp = up;
    camera.updateInput(movement);
    return camera.freePosition() - initial;
}

} // namespace

TEST(ViewerCameraControllerTest, OrbitResetBuildsFiniteVulkanFrame)
{
    ku::OrbitCameraController camera;
    camera.reset(cameraConfig());
    camera.setViewerLayout(viewerLayout());

    const ku::CameraFrame frame = camera.frame();
    EXPECT_EQ(camera.mode(), ku::ViewerCameraMode::OrbitInspect);
    EXPECT_FLOAT_EQ(camera.distance(), 5.0f);
    EXPECT_LT(frame.projection[1][1], 0.0f);
    EXPECT_TRUE(finiteMatrix(frame.viewProjection));
    EXPECT_TRUE(finiteMatrix(frame.inverseViewProjection));
    expectMatrixNear(
        frame.viewProjection * frame.inverseViewProjection,
        glm::mat4(1.0f),
        2e-4f);
    EXPECT_FLOAT_EQ(camera.viewerAspect(), viewerLayout().sceneAspect);
}

TEST(ViewerCameraControllerTest, OrbitLeftDragOnlyRotatesModelAndWheelZooms)
{
    ku::OrbitCameraController camera;
    camera.reset(cameraConfig());
    camera.updateInput(sample());
    const ku::CameraFrame initialFrame = camera.frame();
    const glm::mat4 initialModel = camera.modelMatrix(1.0f, glm::vec3(0.0f));

    ku::CameraInputSample press = sample();
    press.leftDown = true;
    press.leftPressed = true;
    camera.updateInput(press);
    ku::CameraInputSample drag = sample();
    drag.leftDown = true;
    drag.pointerDeltaX = 12.0;
    drag.pointerDeltaY = -5.0;
    camera.updateInput(drag);

    expectMatrixNear(camera.frame().view, initialFrame.view);
    EXPECT_NE(camera.modelMatrix(1.0f, glm::vec3(0.0f))[0][0], initialModel[0][0]);
    const float beforeWheel = camera.distance();
    ku::CameraInputSample wheel = sample();
    wheel.wheelY = 1.0;
    camera.updateInput(wheel);
    EXPECT_LT(camera.distance(), beforeWheel);
}

TEST(ViewerCameraControllerTest, FreeFlyStartsFromCurrentOrbitFrameAndIsFinite)
{
    ku::OrbitCameraController camera;
    camera.reset(cameraConfig());
    camera.setViewerLayout(viewerLayout());
    const ku::CameraFrame orbit = camera.frame();

    camera.setMode(ku::ViewerCameraMode::FreeFly);
    const ku::CameraFrame free = camera.frame();
    expectVecNear(free.position, orbit.position);
    expectMatrixNear(free.view, orbit.view);
    expectMatrixNear(free.projection, orbit.projection);
    EXPECT_TRUE(finiteMatrix(free.viewProjection));
    EXPECT_TRUE(finiteMatrix(free.inverseViewProjection));
}

TEST(ViewerCameraControllerTest, FreeFlySupportsSixDirections)
{
    EXPECT_LT(runMovement(true, false, false, false, false, false).z, 0.0f);
    EXPECT_GT(runMovement(false, true, false, false, false, false).z, 0.0f);
    EXPECT_LT(runMovement(false, false, true, false, false, false).x, 0.0f);
    EXPECT_GT(runMovement(false, false, false, true, false, false).x, 0.0f);
    EXPECT_LT(runMovement(false, false, false, false, true, false).y, 0.0f);
    EXPECT_GT(runMovement(false, false, false, false, false, true).y, 0.0f);
}

TEST(ViewerCameraControllerTest, DiagonalMovementIsNormalized)
{
    const glm::vec3 straight = runMovement(
        true, false, false, false, false, false, 0.1f);
    const glm::vec3 diagonal = runMovement(
        true, false, false, true, false, false, 0.1f);
    EXPECT_NEAR(glm::length(straight), 0.3f, 1e-5f);
    EXPECT_NEAR(glm::length(diagonal), glm::length(straight), 1e-5f);
}

TEST(ViewerCameraControllerTest, MoveSpeedIsPositiveAndClamped)
{
    ku::OrbitCameraController camera;
    camera.reset(cameraConfig());
    EXPECT_FLOAT_EQ(camera.moveSpeed(), 3.0f);
    camera.setMoveSpeed(-10.0f);
    EXPECT_FLOAT_EQ(camera.moveSpeed(), 0.1f);
    camera.setMoveSpeed(1000.0f);
    EXPECT_FLOAT_EQ(camera.moveSpeed(), 50.0f);
    camera.setMoveSpeed(std::numeric_limits<float>::quiet_NaN());
    EXPECT_FLOAT_EQ(camera.moveSpeed(), 50.0f);
}

TEST(ViewerCameraControllerTest, MovementIsFrameRateIndependentAcrossOneSecond)
{
    auto simulate = [](int frameRate) {
        ku::OrbitCameraController camera;
        camera.reset(cameraConfig());
        armFreeInput(camera);
        for (int frame = 0; frame < frameRate; ++frame) {
            ku::CameraInputSample movement = sample();
            movement.deltaTime = 1.0f / static_cast<float>(frameRate);
            movement.moveForward = true;
            camera.updateInput(movement);
        }
        return camera.freePosition();
    };

    const glm::vec3 at30 = simulate(30);
    const glm::vec3 at60 = simulate(60);
    const glm::vec3 at144 = simulate(144);
    expectVecNear(at30, at60, 2e-4f);
    expectVecNear(at60, at144, 2e-4f);
    EXPECT_NEAR(at60.z, 2.0f, 2e-4f);
}

TEST(ViewerCameraControllerTest, InvalidDeltaTimeStopsAndHugeDeltaIsClamped)
{
    EXPECT_NEAR(
        glm::length(runMovement(
            true, false, false, false, false, false,
            std::numeric_limits<float>::quiet_NaN())),
        0.0f,
        1e-6f);
    EXPECT_NEAR(
        glm::length(runMovement(
            true, false, false, false, false, false, -1.0f)),
        0.0f,
        1e-6f);
    EXPECT_NEAR(
        glm::length(runMovement(
            true, false, false, false, false, false, 5.0f)),
        0.3f,
        1e-5f);
}

TEST(ViewerCameraControllerTest, FreeLookUsesRightDragAndClampsPitch)
{
    ku::OrbitCameraController camera;
    camera.reset(cameraConfig());
    camera.setMode(ku::ViewerCameraMode::FreeFly);
    camera.updateInput(sample());

    ku::CameraInputSample press = sample();
    press.rightDown = true;
    press.rightPressed = true;
    camera.updateInput(press);
    ku::CameraInputSample drag = sample();
    drag.rightDown = true;
    drag.pointerDeltaX = 10.0;
    drag.pointerDeltaY = 10.0;
    camera.updateInput(drag);
    EXPECT_GT(camera.freeYaw(), 0.0f);
    EXPECT_LT(camera.freePitch(), 0.0f);

    drag.pointerDeltaY = -100000.0;
    camera.updateInput(drag);
    EXPECT_LT(camera.freePitch(), 1.56f);
    drag.pointerDeltaX = std::numeric_limits<double>::quiet_NaN();
    drag.pointerDeltaY = std::numeric_limits<double>::quiet_NaN();
    const float yaw = camera.freeYaw();
    const float pitch = camera.freePitch();
    camera.updateInput(drag);
    EXPECT_FLOAT_EQ(camera.freeYaw(), yaw);
    EXPECT_FLOAT_EQ(camera.freePitch(), pitch);
    EXPECT_TRUE(std::isfinite(camera.freeYaw()));
}

TEST(ViewerCameraControllerTest, RightPressMustStartInsideSceneButDragMayLeave)
{
    ku::OrbitCameraController camera;
    camera.reset(cameraConfig());
    camera.setMode(ku::ViewerCameraMode::FreeFly);
    camera.updateInput(sample());

    ku::CameraInputSample uiPress = sample();
    uiPress.pointerX = 1000.0;
    uiPress.rightDown = true;
    uiPress.rightPressed = true;
    camera.updateInput(uiPress);
    EXPECT_FALSE(camera.freeInputFocused());
    camera.updateInput(sample());

    ku::CameraInputSample scenePress = sample();
    scenePress.rightDown = true;
    scenePress.rightPressed = true;
    camera.updateInput(scenePress);
    EXPECT_TRUE(camera.freeInputFocused());
    const float yaw = camera.freeYaw();

    ku::CameraInputSample outsideDrag = sample();
    outsideDrag.pointerX = 1100.0;
    outsideDrag.rightDown = true;
    outsideDrag.pointerDeltaX = 10.0;
    outsideDrag.uiCapturesMouse = true;
    camera.updateInput(outsideDrag);
    EXPECT_GT(camera.freeYaw(), yaw);
}

TEST(ViewerCameraControllerTest, ModeSwitchesPreserveFrameAndModelRotation)
{
    ku::OrbitCameraController camera;
    camera.reset(cameraConfig());
    camera.addRotation(0.4f, -0.2f);
    const glm::mat4 model = camera.modelMatrix(1.0f, glm::vec3(0.0f));
    const ku::CameraFrame orbit = camera.frame();

    camera.setMode(ku::ViewerCameraMode::FreeFly);
    const ku::CameraFrame free = camera.frame();
    expectVecNear(free.position, orbit.position);
    expectMatrixNear(free.view, orbit.view);
    expectMatrixNear(camera.modelMatrix(1.0f, glm::vec3(0.0f)), model);

    armFreeInput(camera);
    ku::CameraInputSample move = sample();
    move.deltaTime = 0.1f;
    move.moveForward = true;
    move.moveRight = true;
    camera.updateInput(move);
    ku::CameraInputSample lookPress = sample();
    lookPress.rightDown = true;
    lookPress.rightPressed = true;
    camera.updateInput(lookPress);
    ku::CameraInputSample lookDrag = sample();
    lookDrag.rightDown = true;
    lookDrag.pointerDeltaX = 13.0;
    lookDrag.pointerDeltaY = -7.0;
    camera.updateInput(lookDrag);
    const ku::CameraFrame movedFree = camera.frame();

    camera.setMode(ku::ViewerCameraMode::OrbitInspect);
    const ku::CameraFrame orbitAgain = camera.frame();
    expectVecNear(orbitAgain.position, movedFree.position);
    expectMatrixNear(orbitAgain.view, movedFree.view);
    expectMatrixNear(camera.modelMatrix(1.0f, glm::vec3(0.0f)), model);
}

TEST(ViewerCameraControllerTest, RepeatedModeSwitchDoesNotDrift)
{
    ku::OrbitCameraController camera;
    camera.reset(cameraConfig());
    const ku::CameraFrame initial = camera.frame();
    for (int iteration = 0; iteration < 100; ++iteration) {
        camera.setMode(ku::ViewerCameraMode::FreeFly);
        camera.setMode(ku::ViewerCameraMode::OrbitInspect);
    }
    const ku::CameraFrame final = camera.frame();
    expectVecNear(final.position, initial.position, 2e-4f);
    expectMatrixNear(final.view, initial.view, 2e-4f);
    EXPECT_TRUE(finiteMatrix(final.viewProjection));
}

TEST(ViewerCameraControllerTest, UiKeyboardAndTextCaptureClearFocusAndBlockMovement)
{
    ku::OrbitCameraController camera;
    camera.reset(cameraConfig());
    armFreeInput(camera);
    const glm::vec3 initial = camera.freePosition();

    ku::CameraInputSample captured = sample();
    captured.deltaTime = 0.1f;
    captured.moveForward = true;
    captured.uiCapturesKeyboard = true;
    camera.updateInput(captured);
    expectVecNear(camera.freePosition(), initial);
    EXPECT_FALSE(camera.freeInputFocused());

    captured.uiCapturesKeyboard = false;
    captured.uiWantsTextInput = true;
    camera.updateInput(captured);
    expectVecNear(camera.freePosition(), initial);

    camera.updateInput(sample());
    ku::CameraInputSample mouseCaptured = sample();
    mouseCaptured.rightDown = true;
    mouseCaptured.rightPressed = true;
    mouseCaptured.uiCapturesMouse = true;
    camera.updateInput(mouseCaptured);
    EXPECT_FALSE(camera.freeInputFocused());
}

TEST(ViewerCameraControllerTest, StartupHeldKeyRequiresReleaseAndNewSceneFocus)
{
    ku::OrbitCameraController camera;
    camera.reset(cameraConfig());
    camera.setMode(ku::ViewerCameraMode::FreeFly);
    const glm::vec3 initial = camera.freePosition();

    ku::CameraInputSample heldAtStartup = sample();
    heldAtStartup.deltaTime = 0.1f;
    heldAtStartup.moveForward = true;
    camera.updateInput(heldAtStartup);
    expectVecNear(camera.freePosition(), initial);

    camera.updateInput(sample());
    camera.updateInput(heldAtStartup);
    expectVecNear(camera.freePosition(), initial);

    armFreeInput(camera);
    camera.updateInput(heldAtStartup);
    EXPECT_LT(camera.freePosition().z, initial.z);
}

TEST(ViewerCameraControllerTest, FocusLossEpochAndModeSwitchRequireHeldRelease)
{
    ku::OrbitCameraController camera;
    camera.reset(cameraConfig());
    armFreeInput(camera, 10);
    const glm::vec3 initial = camera.freePosition();

    ku::CameraInputSample resumed = sample(12);
    resumed.deltaTime = 0.1f;
    resumed.rightDown = true;
    resumed.moveForward = true;
    camera.updateInput(resumed);
    expectVecNear(camera.freePosition(), initial);
    EXPECT_FALSE(camera.freeInputFocused());

    camera.updateInput(sample(12));
    ku::CameraInputSample heldWithoutFocus = sample(12);
    heldWithoutFocus.deltaTime = 0.1f;
    heldWithoutFocus.moveForward = true;
    camera.updateInput(heldWithoutFocus);
    expectVecNear(camera.freePosition(), initial);

    armFreeInput(camera, 12);
    const glm::vec3 beforeSwitch = camera.freePosition();
    camera.setMode(ku::ViewerCameraMode::OrbitInspect);
    camera.setMode(ku::ViewerCameraMode::FreeFly);
    ku::CameraInputSample heldAfterSwitch = sample(12);
    heldAfterSwitch.rightDown = true;
    heldAfterSwitch.moveForward = true;
    heldAfterSwitch.deltaTime = 0.1f;
    camera.updateInput(heldAfterSwitch);
    expectVecNear(camera.freePosition(), beforeSwitch);
}

TEST(ViewerCameraControllerTest, NoSceneOrInactiveWindowCannotFocusOrMove)
{
    ku::OrbitCameraController camera;
    camera.reset(cameraConfig());
    camera.setMode(ku::ViewerCameraMode::FreeFly);
    const glm::vec3 initial = camera.freePosition();

    ku::CameraInputSample invalid = sample();
    invalid.viewerLayout = {};
    invalid.rightDown = true;
    invalid.rightPressed = true;
    invalid.moveForward = true;
    invalid.deltaTime = 0.1f;
    camera.updateInput(invalid);
    EXPECT_FALSE(camera.freeInputFocused());
    expectVecNear(camera.freePosition(), initial);

    invalid = sample(2);
    invalid.windowActive = false;
    camera.updateInput(invalid);
    EXPECT_FALSE(camera.freeInputFocused());
    expectVecNear(camera.freePosition(), initial);
}

TEST(ViewerCameraControllerTest, ResetViewKeepsModeAndModelButRestoresPose)
{
    ku::OrbitCameraController camera;
    camera.reset(cameraConfig());
    camera.addRotation(0.5f, 0.2f);
    const glm::mat4 model = camera.modelMatrix(1.0f, glm::vec3(0.0f));
    armFreeInput(camera);

    ku::CameraInputSample movement = sample();
    movement.deltaTime = 0.1f;
    movement.moveForward = true;
    camera.updateInput(movement);
    EXPECT_NE(camera.freePosition().z, 5.0f);

    camera.resetView();
    EXPECT_EQ(camera.mode(), ku::ViewerCameraMode::FreeFly);
    expectVecNear(camera.frame().position, cameraConfig().position);
    expectMatrixNear(camera.modelMatrix(1.0f, glm::vec3(0.0f)), model);

    camera.resetRotation();
    expectMatrixNear(
        camera.modelMatrix(1.0f, glm::vec3(0.0f)),
        glm::mat4(1.0f));
}

TEST(ViewerCameraControllerTest, ResetViewRequiresHeldRightReleaseBeforeLook)
{
    ku::OrbitCameraController camera;
    camera.reset(cameraConfig());
    armFreeInput(camera);

    ku::CameraInputSample press = sample();
    press.rightDown = true;
    press.rightPressed = true;
    camera.updateInput(press);
    camera.resetView();
    const float resetYaw = camera.freeYaw();

    ku::CameraInputSample held = sample();
    held.rightDown = true;
    held.pointerDeltaX = 20.0;
    camera.updateInput(held);
    EXPECT_FLOAT_EQ(camera.freeYaw(), resetYaw);
    EXPECT_FALSE(camera.freeInputFocused());

    camera.updateInput(sample());
    camera.updateInput(press);
    held.pointerDeltaX = 20.0;
    camera.updateInput(held);
    EXPECT_GT(camera.freeYaw(), resetYaw);
}
