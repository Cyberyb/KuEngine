#include "OrbitCameraController.h"

#include <KuEngine/Core/Input.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>

namespace ku {

void OrbitCameraController::reset(const asset::SceneCameraConfig& config)
{
    m_initialPosition = config.position;
    m_target = config.target;
    m_up = config.up;
    m_fovYDegrees = config.fovYDeg;
    m_nearPlane = config.nearPlane;
    m_farPlane = config.farPlane;

    const float configuredDistance = glm::length(m_initialPosition - m_target);
    m_distance = configuredDistance > 0.001f ? configuredDistance : 4.0f;
    m_yaw = 0.0f;
    m_pitch = 0.0f;
    m_dragging = false;
}

void OrbitCameraController::updateInput(
    bool wantCaptureMouse,
    float mouseWheel)
{
    if (!wantCaptureMouse && mouseWheel != 0.0f) {
        const float zoomStep = std::max(0.1f, m_distance * 0.1f);
        m_distance = std::clamp(
            m_distance - mouseWheel * zoomStep,
            2.0f,
            12.0f);
    }

    const bool leftDown =
        Input::isMouseButtonDown(Input::MOUSE_BUTTON_LEFT);
    if (!leftDown) {
        m_dragging = false;
        return;
    }

    if (Input::isMouseButtonPressed(Input::MOUSE_BUTTON_LEFT)) {
        m_dragging = !wantCaptureMouse;
        return;
    }

    if (m_dragging && !wantCaptureMouse) {
        addRotation(
            Input::mouseDeltaX() * 0.01f,
            Input::mouseDeltaY() * 0.01f);
    }
}

void OrbitCameraController::addRotation(
    float deltaYaw,
    float deltaPitch)
{
    m_yaw += deltaYaw;
    m_pitch = std::clamp(m_pitch + deltaPitch, -1.5f, 1.5f);
}

void OrbitCameraController::resetRotation()
{
    m_yaw = 0.0f;
    m_pitch = 0.0f;
}

void OrbitCameraController::onResize(uint32_t width, uint32_t height)
{
    if (width == 0 || height == 0) {
        return;
    }

    m_viewportWidth = width;
    m_viewportHeight = height;
    m_aspect = static_cast<float>(width) / static_cast<float>(height);
}

void OrbitCameraController::setViewportRegion(
    float offsetX,
    float visibleWidth)
{
    m_offsetX = std::clamp(offsetX, 0.0f, 1.0f);
    m_visibleWidth = std::clamp(
        visibleWidth,
        0.0f,
        1.0f - m_offsetX);
}

void OrbitCameraController::setProjection(
    float fovYDegrees,
    float nearPlane,
    float farPlane)
{
    m_fovYDegrees = std::clamp(fovYDegrees, 1.0f, 179.0f);
    m_nearPlane = std::max(0.001f, nearPlane);
    m_farPlane = std::max(m_nearPlane + 0.1f, farPlane);
}

OrbitViewportRegion OrbitCameraController::viewportRegion() const
{
    OrbitViewportRegion region{};
    region.x = static_cast<float>(m_viewportWidth) * m_offsetX;
    region.width =
        static_cast<float>(m_viewportWidth) * m_visibleWidth;
    region.scissorX =
        static_cast<uint32_t>(std::floor(region.x));
    const uint32_t renderRight = std::min(
        m_viewportWidth,
        static_cast<uint32_t>(std::ceil(region.x + region.width)));
    region.scissorWidth = std::max(1u, renderRight - region.scissorX);
    region.height = m_viewportHeight;
    return region;
}

OrbitCameraFrame OrbitCameraController::frame() const
{
    glm::vec3 cameraForward = m_initialPosition - m_target;
    if (glm::length(cameraForward) < 1e-4f) {
        cameraForward = glm::vec3(0.0f, 0.0f, 1.0f);
    } else {
        cameraForward = glm::normalize(cameraForward);
    }

    glm::vec3 cameraUp = m_up;
    if (glm::length(cameraUp) < 1e-4f) {
        cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);
    } else {
        cameraUp = glm::normalize(cameraUp);
    }

    OrbitCameraFrame result{};
    result.position =
        m_target + cameraForward * std::max(m_distance, 0.1f);
    result.view = glm::lookAt(result.position, m_target, cameraUp);
    result.projection = glm::perspective(
        glm::radians(std::clamp(m_fovYDegrees, 1.0f, 179.0f)),
        std::max(m_aspect, 0.01f),
        std::max(m_nearPlane, 0.001f),
        std::max(m_farPlane, m_nearPlane + 0.1f));
    result.projection[0][0] /= std::max(m_visibleWidth, 0.001f);
    result.projection[2][0] =
        (2.0f * m_offsetX + m_visibleWidth - 1.0f)
        / std::max(m_visibleWidth, 0.001f);
    result.projection[1][1] *= -1.0f;
    result.viewProjection = result.projection * result.view;
    result.inverseViewProjection = glm::inverse(result.viewProjection);
    return result;
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
