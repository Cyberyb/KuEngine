#include "OrbitCameraController.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>

namespace ku {
namespace {

constexpr float kMinimumOrbitDistance = 2.0f;
constexpr float kMaximumOrbitDistance = 12.0f;
constexpr float kMinimumMoveSpeed = 0.1f;
constexpr float kMaximumMoveSpeed = 50.0f;
constexpr float kMouseSensitivity = 0.01f;
constexpr float kMaximumPitch = 1.55334306f; // 89 degrees
constexpr float kTwoPi = 6.28318530718f;

bool finiteVector(const glm::vec3& value) noexcept
{
    return std::isfinite(value.x)
        && std::isfinite(value.y)
        && std::isfinite(value.z);
}

glm::vec3 safeNormalized(
    const glm::vec3& value,
    const glm::vec3& fallback) noexcept
{
    if (!finiteVector(value)) {
        return fallback;
    }
    const float length = glm::length(value);
    return std::isfinite(length) && length > 1e-5f
        ? value / length
        : fallback;
}

float wrapYaw(float yaw) noexcept
{
    if (!std::isfinite(yaw)) {
        return 0.0f;
    }
    return std::remainder(yaw, kTwoPi);
}

float safeDeltaTime(float deltaTime) noexcept
{
    if (!std::isfinite(deltaTime) || deltaTime <= 0.0f) {
        return 0.0f;
    }
    return std::min(deltaTime, 0.1f);
}

} // namespace

void OrbitCameraController::reset(const asset::SceneCameraConfig& config)
{
    m_resetConfig = config;
    m_initialPosition = config.position;
    m_target = config.target;
    m_up = safeNormalized(config.up, glm::vec3(0.0f, 1.0f, 0.0f));
    m_fovYDegrees = std::isfinite(config.fovYDeg) ? config.fovYDeg : 60.0f;
    m_nearPlane = std::isfinite(config.nearPlane) ? config.nearPlane : 0.1f;
    m_farPlane = std::isfinite(config.farPlane) ? config.farPlane : 200.0f;
    setProjection(m_fovYDegrees, m_nearPlane, m_farPlane);

    const float configuredDistance = glm::length(m_initialPosition - m_target);
    m_distance = configuredDistance > 0.001f ? configuredDistance : 4.0f;
    m_orbitDirection = safeNormalized(
        m_initialPosition - m_target,
        glm::vec3(0.0f, 0.0f, 1.0f));
    m_yaw = 0.0f;
    m_pitch = 0.0f;
    m_moveSpeed = 3.0f;
    m_mode = ViewerCameraMode::OrbitInspect;
    seedFreeFromFrame(orbitFrame());
    invalidateInput();
}

void OrbitCameraController::resetView()
{
    const ViewerCameraMode retainedMode = m_mode;
    m_initialPosition = m_resetConfig.position;
    m_target = m_resetConfig.target;
    m_up = safeNormalized(
        m_resetConfig.up,
        glm::vec3(0.0f, 1.0f, 0.0f));
    const float configuredDistance = glm::length(m_initialPosition - m_target);
    m_distance = configuredDistance > 0.001f ? configuredDistance : 4.0f;
    m_orbitDirection = safeNormalized(
        m_initialPosition - m_target,
        glm::vec3(0.0f, 0.0f, 1.0f));
    if (retainedMode == ViewerCameraMode::FreeFly) {
        seedFreeFromFrame(orbitFrame());
    }
    invalidateInput();
}

void OrbitCameraController::updateInput(const CameraInputSample& input)
{
    setViewerLayout(input.viewerLayout);

    if (!m_epochInitialized) {
        m_epochInitialized = true;
        m_interactionEpoch = input.interactionEpoch;
    } else if (input.interactionEpoch != m_interactionEpoch) {
        m_interactionEpoch = input.interactionEpoch;
        invalidateInput();
        m_epochInitialized = true;
        m_interactionEpoch = input.interactionEpoch;
    }

    if (!input.windowActive || !input.viewerLayout.sceneInputVisible()) {
        invalidateInput();
        m_epochInitialized = true;
        m_interactionEpoch = input.interactionEpoch;
        return;
    }

    if (m_waitForNeutralInput) {
        if (relevantInputHeld(input)) {
            return;
        }
        m_waitForNeutralInput = false;
    }

    if (m_mode == ViewerCameraMode::OrbitInspect) {
        updateOrbit(input);
    } else {
        updateFree(input);
    }
}

void OrbitCameraController::updateOrbit(
    const CameraInputSample& input) noexcept
{
    const ScenePointerAction action = m_orbitInteraction.update(
        input.viewerLayout,
        ScenePointerSample{
            .logicalX = input.pointerX,
            .logicalY = input.pointerY,
            .deltaX = input.pointerDeltaX,
            .deltaY = input.pointerDeltaY,
            .scrollY = input.wheelY,
            .primaryDown = input.leftDown,
            .primaryPressed = input.leftPressed,
            .windowActive = input.windowActive,
            .uiCapturesPointer = input.uiCapturesMouse,
            .interactionEpoch = input.interactionEpoch,
        });
    if (action.scrollY != 0.0) {
        const float zoomStep = std::max(0.1f, m_distance * 0.1f);
        m_distance = std::clamp(
            m_distance - static_cast<float>(action.scrollY) * zoomStep,
            kMinimumOrbitDistance,
            kMaximumOrbitDistance);
    }

    if (action.rotate) {
        addRotation(
            static_cast<float>(action.rotationDeltaX) * kMouseSensitivity,
            static_cast<float>(action.rotationDeltaY) * kMouseSensitivity);
    }
}

void OrbitCameraController::updateFree(
    const CameraInputSample& input) noexcept
{
    if ((input.uiCapturesKeyboard || input.uiWantsTextInput)
        || (input.uiCapturesMouse && !m_freeLookInteraction.dragging())) {
        m_freeInputFocused = false;
        m_freeLookInteraction.reset();
        m_waitForNeutralInput = true;
        return;
    }

    const ScenePointerAction look = m_freeLookInteraction.update(
        input.viewerLayout,
        ScenePointerSample{
            .logicalX = input.pointerX,
            .logicalY = input.pointerY,
            .deltaX = input.pointerDeltaX,
            .deltaY = input.pointerDeltaY,
            .primaryDown = input.rightDown,
            .primaryPressed = input.rightPressed,
            .windowActive = input.windowActive,
            .uiCapturesPointer = input.uiCapturesMouse,
            .interactionEpoch = input.interactionEpoch,
        });
    if (input.rightPressed && m_freeLookInteraction.dragging()) {
        m_freeInputFocused = true;
    }
    if (look.rotate) {
        const float deltaYaw = std::isfinite(look.rotationDeltaX)
            ? static_cast<float>(look.rotationDeltaX) * kMouseSensitivity
            : 0.0f;
        const float deltaPitch = std::isfinite(look.rotationDeltaY)
            ? static_cast<float>(look.rotationDeltaY) * kMouseSensitivity
            : 0.0f;
        m_freeYaw = wrapYaw(m_freeYaw + deltaYaw);
        m_freePitch = std::clamp(
            m_freePitch - deltaPitch,
            -kMaximumPitch,
            kMaximumPitch);
    }

    if (!m_freeInputFocused) {
        return;
    }

    glm::vec3 movement{0.0f};
    const glm::vec3 forward = freeForward();
    const glm::vec3 right = safeNormalized(
        glm::cross(forward, glm::vec3(0.0f, 1.0f, 0.0f)),
        glm::vec3(1.0f, 0.0f, 0.0f));
    movement += forward
        * static_cast<float>(input.moveForward - input.moveBackward);
    movement += right
        * static_cast<float>(input.moveRight - input.moveLeft);
    movement += glm::vec3(0.0f, 1.0f, 0.0f)
        * static_cast<float>(input.moveUp - input.moveDown);

    const float movementLength = glm::length(movement);
    if (std::isfinite(movementLength) && movementLength > 1e-5f) {
        movement /= movementLength;
        m_freePosition += movement * m_moveSpeed * safeDeltaTime(input.deltaTime);
        if (!finiteVector(m_freePosition)) {
            m_freePosition = m_initialPosition;
        }
    }
}

void OrbitCameraController::setViewerLayout(
    const ViewerLayout& layout) noexcept
{
    if (std::isfinite(layout.sceneAspect) && layout.sceneAspect > 0.0f) {
        m_aspect = layout.sceneAspect;
    } else {
        m_aspect = 1.0f;
    }
}

void OrbitCameraController::addRotation(
    float deltaYaw,
    float deltaPitch)
{
    if (std::isfinite(deltaYaw)) {
        m_yaw = wrapYaw(m_yaw + deltaYaw);
    }
    if (std::isfinite(deltaPitch)) {
        m_pitch = std::clamp(m_pitch + deltaPitch, -1.5f, 1.5f);
    }
}

void OrbitCameraController::resetRotation()
{
    m_yaw = 0.0f;
    m_pitch = 0.0f;
}

void OrbitCameraController::setProjection(
    float fovYDegrees,
    float nearPlane,
    float farPlane)
{
    m_fovYDegrees = std::isfinite(fovYDegrees)
        ? std::clamp(fovYDegrees, 1.0f, 179.0f)
        : 60.0f;
    m_nearPlane = std::isfinite(nearPlane)
        ? std::max(0.001f, nearPlane)
        : 0.1f;
    m_farPlane = std::isfinite(farPlane)
        ? std::max(m_nearPlane + 0.1f, farPlane)
        : std::max(m_nearPlane + 0.1f, 200.0f);
}

void OrbitCameraController::setMoveSpeed(float unitsPerSecond) noexcept
{
    if (std::isfinite(unitsPerSecond)) {
        m_moveSpeed = std::clamp(
            unitsPerSecond,
            kMinimumMoveSpeed,
            kMaximumMoveSpeed);
    }
}

void OrbitCameraController::setMode(ViewerCameraMode mode) noexcept
{
    if (mode == m_mode) {
        return;
    }

    const CameraFrame source = frame();
    if (mode == ViewerCameraMode::FreeFly) {
        seedFreeFromFrame(source);
    } else {
        seedOrbitFromFrame(source);
    }
    m_mode = mode;
    invalidateInput();
}

CameraFrame OrbitCameraController::frame() const
{
    return m_mode == ViewerCameraMode::OrbitInspect
        ? orbitFrame()
        : freeFrame();
}

CameraFrame OrbitCameraController::orbitFrame() const
{
    CameraFrame result{};
    result.position =
        m_target + m_orbitDirection * std::max(m_distance, 0.1f);
    result.view = glm::lookAt(result.position, m_target, m_up);
    result.projection = glm::perspective(
        glm::radians(std::clamp(m_fovYDegrees, 1.0f, 179.0f)),
        std::max(m_aspect, 0.01f),
        std::max(m_nearPlane, 0.001f),
        std::max(m_farPlane, m_nearPlane + 0.1f));
    result.projection[1][1] *= -1.0f;
    result.viewProjection = result.projection * result.view;
    result.inverseViewProjection = glm::inverse(result.viewProjection);
    return result;
}

CameraFrame OrbitCameraController::freeFrame() const
{
    CameraFrame result{};
    result.position = m_freePosition;
    result.view = glm::lookAt(
        m_freePosition,
        m_freePosition + freeForward(),
        glm::vec3(0.0f, 1.0f, 0.0f));
    result.projection = glm::perspective(
        glm::radians(std::clamp(m_fovYDegrees, 1.0f, 179.0f)),
        std::max(m_aspect, 0.01f),
        std::max(m_nearPlane, 0.001f),
        std::max(m_farPlane, m_nearPlane + 0.1f));
    result.projection[1][1] *= -1.0f;
    result.viewProjection = result.projection * result.view;
    result.inverseViewProjection = glm::inverse(result.viewProjection);
    return result;
}

glm::vec3 OrbitCameraController::freeForward() const noexcept
{
    const float cosPitch = std::cos(m_freePitch);
    return safeNormalized(
        glm::vec3(
            cosPitch * std::sin(m_freeYaw),
            std::sin(m_freePitch),
            -cosPitch * std::cos(m_freeYaw)),
        glm::vec3(0.0f, 0.0f, -1.0f));
}

void OrbitCameraController::seedFreeFromFrame(
    const CameraFrame& source) noexcept
{
    m_freePosition = finiteVector(source.position)
        ? source.position
        : m_initialPosition;
    const glm::vec3 forward = safeNormalized(
        -glm::vec3(source.view[0][2], source.view[1][2], source.view[2][2]),
        safeNormalized(m_target - m_freePosition, glm::vec3(0.0f, 0.0f, -1.0f)));
    m_freePitch = std::clamp(
        std::asin(std::clamp(forward.y, -1.0f, 1.0f)),
        -kMaximumPitch,
        kMaximumPitch);
    m_freeYaw = wrapYaw(std::atan2(forward.x, -forward.z));
}

void OrbitCameraController::seedOrbitFromFrame(
    const CameraFrame& source) noexcept
{
    const glm::vec3 position = finiteVector(source.position)
        ? source.position
        : m_initialPosition;
    const glm::vec3 forward = freeForward();
    m_distance = std::clamp(
        std::isfinite(m_distance) ? m_distance : 4.0f,
        kMinimumOrbitDistance,
        kMaximumOrbitDistance);
    m_target = position + forward * m_distance;
    m_orbitDirection = -forward;
    m_up = glm::vec3(0.0f, 1.0f, 0.0f);
}

void OrbitCameraController::invalidateInput() noexcept
{
    m_orbitInteraction.reset();
    m_freeLookInteraction.reset();
    m_freeInputFocused = false;
    m_waitForNeutralInput = true;
}

bool OrbitCameraController::relevantInputHeld(
    const CameraInputSample& input) const noexcept
{
    return input.leftDown
        || input.rightDown
        || input.moveForward
        || input.moveBackward
        || input.moveLeft
        || input.moveRight
        || input.moveDown
        || input.moveUp;
}

glm::mat4 OrbitCameraController::modelMatrix(
    float fitScale,
    const glm::vec3& modelCenter) const
{
    glm::mat4 model{1.0f};
    model = glm::rotate(
        model,
        m_yaw,
        glm::vec3(0.0f, 1.0f, 0.0f));
    model = glm::rotate(
        model,
        m_pitch,
        glm::vec3(1.0f, 0.0f, 0.0f));
    model = glm::scale(model, glm::vec3(fitScale));
    return glm::translate(model, -modelCenter);
}

} // namespace ku
