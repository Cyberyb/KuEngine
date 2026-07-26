// Mclaren 轨道相机控制器：封装输入、视口裁剪、投影参数和模型观察旋转。
#pragma once

#include <cstdint>

#ifndef GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#endif
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include <KuEngine/Asset/AssetConfig.h>

namespace ku {

struct OrbitCameraFrame {
    glm::vec3 position{0.0f, 0.0f, 4.0f};
    glm::mat4 view{1.0f};
    glm::mat4 projection{1.0f};
    glm::mat4 viewProjection{1.0f};
    glm::mat4 inverseViewProjection{1.0f};
};

struct OrbitViewportRegion {
    float x = 0.0f;
    float width = 1.0f;
    uint32_t scissorX = 0;
    uint32_t scissorWidth = 1;
    uint32_t height = 1;
};

class OrbitCameraController {
public:
    void reset(const asset::SceneCameraConfig& config);
    void updateInput(bool wantCaptureMouse, float mouseWheel);
    void addRotation(float deltaYaw, float deltaPitch);
    void resetRotation();
    void onResize(uint32_t width, uint32_t height);

    [[nodiscard]] OrbitViewportRegion viewportRegion() const;
    [[nodiscard]] OrbitCameraFrame frame() const;
    [[nodiscard]] glm::mat4 modelMatrix(
        float fitScale,
        const glm::vec3& modelCenter) const;

    void setViewportRegion(float offsetX, float visibleWidth);
    void setProjection(float fovYDegrees, float nearPlane, float farPlane);

    [[nodiscard]] float distance() const { return m_distance; }
    [[nodiscard]] float yaw() const { return m_yaw; }
    [[nodiscard]] float pitch() const { return m_pitch; }
    [[nodiscard]] float viewportOffsetX() const { return m_offsetX; }
    [[nodiscard]] float visibleWidth() const { return m_visibleWidth; }
    [[nodiscard]] float fovYDegrees() const { return m_fovYDegrees; }
    [[nodiscard]] float nearPlane() const { return m_nearPlane; }
    [[nodiscard]] float farPlane() const { return m_farPlane; }

private:
    glm::vec3 m_initialPosition{0.0f, 0.0f, 4.0f};
    glm::vec3 m_target{0.0f, 0.0f, 0.0f};
    glm::vec3 m_up{0.0f, 1.0f, 0.0f};

    float m_distance = 4.0f;
    float m_yaw = 0.0f;
    float m_pitch = 0.0f;
    bool m_dragging = false;

    float m_offsetX = 0.0f;
    float m_visibleWidth = 1.0f;
    float m_fovYDegrees = 60.0f;
    float m_nearPlane = 0.1f;
    float m_farPlane = 200.0f;
    float m_aspect = 16.0f / 9.0f;
    uint32_t m_viewportWidth = 1280;
    uint32_t m_viewportHeight = 720;
};

} // namespace ku
