// Mclaren viewer camera controller. Kept in the example layer until the
// common scene/camera work planned for M2.
#pragma once

#include <cstdint>

#ifndef GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#endif
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include <KuEngine/Asset/AssetConfig.h>
#include <KuEngine/Core/SceneInteraction.h>

namespace ku {

enum class ViewerCameraMode {
    OrbitInspect,
    FreeFly,
};

struct CameraFrame {
    glm::vec3 position{0.0f, 0.0f, 4.0f};
    glm::mat4 view{1.0f};
    glm::mat4 projection{1.0f};
    glm::mat4 viewProjection{1.0f};
    glm::mat4 inverseViewProjection{1.0f};
};

struct CameraInputSample {
    ViewerLayout viewerLayout{};
    float deltaTime = 0.0f;
    double pointerX = 0.0;
    double pointerY = 0.0;
    double pointerDeltaX = 0.0;
    double pointerDeltaY = 0.0;
    double wheelY = 0.0;
    bool leftDown = false;
    bool leftPressed = false;
    bool rightDown = false;
    bool rightPressed = false;
    bool moveForward = false;
    bool moveBackward = false;
    bool moveLeft = false;
    bool moveRight = false;
    bool moveDown = false;
    bool moveUp = false;
    bool windowActive = true;
    bool uiCapturesMouse = false;
    bool uiCapturesKeyboard = false;
    bool uiWantsTextInput = false;
    uint64_t interactionEpoch = 0;
};

class OrbitCameraController {
public:
    void reset(const asset::SceneCameraConfig& config);
    void resetView();
    void updateInput(const CameraInputSample& input);
    void setViewerLayout(const ViewerLayout& layout) noexcept;
    void setMode(ViewerCameraMode mode) noexcept;
    void addRotation(float deltaYaw, float deltaPitch);
    void resetRotation();

    [[nodiscard]] CameraFrame frame() const;
    [[nodiscard]] glm::mat4 modelMatrix(
        float fitScale,
        const glm::vec3& modelCenter) const;

    void setProjection(float fovYDegrees, float nearPlane, float farPlane);
    void setMoveSpeed(float unitsPerSecond) noexcept;

    [[nodiscard]] ViewerCameraMode mode() const noexcept { return m_mode; }
    [[nodiscard]] float distance() const { return m_distance; }
    // Orbit model-inspection rotation; never reused as FreeFly orientation.
    [[nodiscard]] float yaw() const { return m_yaw; }
    [[nodiscard]] float pitch() const { return m_pitch; }
    [[nodiscard]] const glm::vec3& freePosition() const noexcept
    {
        return m_freePosition;
    }
    [[nodiscard]] float freeYaw() const noexcept { return m_freeYaw; }
    [[nodiscard]] float freePitch() const noexcept { return m_freePitch; }
    [[nodiscard]] float moveSpeed() const noexcept { return m_moveSpeed; }
    [[nodiscard]] bool freeInputFocused() const noexcept
    {
        return m_freeInputFocused;
    }
    [[nodiscard]] float viewerAspect() const { return m_aspect; }
    [[nodiscard]] float fovYDegrees() const { return m_fovYDegrees; }
    [[nodiscard]] float nearPlane() const { return m_nearPlane; }
    [[nodiscard]] float farPlane() const { return m_farPlane; }

private:
    [[nodiscard]] CameraFrame orbitFrame() const;
    [[nodiscard]] CameraFrame freeFrame() const;
    [[nodiscard]] glm::vec3 freeForward() const noexcept;
    void seedFreeFromFrame(const CameraFrame& source) noexcept;
    void seedOrbitFromFrame(const CameraFrame& source) noexcept;
    void invalidateInput() noexcept;
    [[nodiscard]] bool relevantInputHeld(
        const CameraInputSample& input) const noexcept;
    void updateOrbit(const CameraInputSample& input) noexcept;
    void updateFree(const CameraInputSample& input) noexcept;

    asset::SceneCameraConfig m_resetConfig{};
    ViewerCameraMode m_mode = ViewerCameraMode::OrbitInspect;
    glm::vec3 m_initialPosition{0.0f, 0.0f, 4.0f};
    glm::vec3 m_target{0.0f, 0.0f, 0.0f};
    glm::vec3 m_up{0.0f, 1.0f, 0.0f};
    glm::vec3 m_orbitDirection{0.0f, 0.0f, 1.0f};

    float m_distance = 4.0f;
    float m_yaw = 0.0f;
    float m_pitch = 0.0f;

    glm::vec3 m_freePosition{0.0f, 0.0f, 4.0f};
    float m_freeYaw = 0.0f;
    float m_freePitch = 0.0f;
    float m_moveSpeed = 3.0f;

    float m_fovYDegrees = 60.0f;
    float m_nearPlane = 0.1f;
    float m_farPlane = 200.0f;
    float m_aspect = 16.0f / 9.0f;
    SceneInteractionGate m_orbitInteraction;
    SceneInteractionGate m_freeLookInteraction;
    bool m_freeInputFocused = false;
    bool m_waitForNeutralInput = true;
    bool m_epochInitialized = false;
    uint64_t m_interactionEpoch = 0;
};

} // namespace ku
