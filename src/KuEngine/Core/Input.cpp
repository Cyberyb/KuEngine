#include "Input.h"

#include <GLFW/glfw3.h>

namespace ku {

// static member definitions
GLFWwindow* Input::s_window = nullptr;
std::array<bool, 512> Input::s_keyDown{};
std::array<bool, 512> Input::s_keyPressed{};
std::array<bool, 8>   Input::s_mouseDown{};
std::array<bool, 8>   Input::s_mousePressed{};
double  Input::s_mouseX = 0;
double  Input::s_mouseY = 0;
double  Input::s_mouseDeltaX = 0;
double  Input::s_mouseDeltaY = 0;
double  Input::s_lastMouseX = 0;
double  Input::s_lastMouseY = 0;
double  Input::s_mouseWheelY = 0;
double  Input::s_pendingMouseWheelY = 0;
bool    Input::s_active = false;
bool    Input::s_interactionActive = false;
uint64_t Input::s_interactionEpoch = 0;

void Input::attach(GLFWwindow* window)
{
    s_window = window;
    ++s_interactionEpoch;
    clearState();
    if (window == nullptr) {
        return;
    }

    glfwGetCursorPos(window, &s_mouseX, &s_mouseY);
    s_lastMouseX = s_mouseX;
    s_lastMouseY = s_mouseY;
    glfwSetScrollCallback(window, scrollCallback);
}

void Input::detach(GLFWwindow* window)
{
    if (window != nullptr && window == s_window) {
        glfwSetScrollCallback(window, nullptr);
        s_window = nullptr;
        ++s_interactionEpoch;
    }
    clearState();
}

void Input::update(GLFWwindow* window, bool active)
{
    const bool wasActive = s_active;
    s_active = active && window != nullptr;

    s_lastMouseX = s_mouseX;
    s_lastMouseY = s_mouseY;
    if (window != nullptr) {
        glfwGetCursorPos(window, &s_mouseX, &s_mouseY);
    }

    if (!s_active) {
        if (wasActive) {
            ++s_interactionEpoch;
        }
        clearState();
        s_lastMouseX = s_mouseX;
        s_lastMouseY = s_mouseY;
        return;
    }

    const bool resumed = !wasActive;
    if (resumed) {
        ++s_interactionEpoch;
    }
    s_interactionActive = !resumed;
    s_mouseDeltaX = resumed ? 0.0 : s_mouseX - s_lastMouseX;
    s_mouseDeltaY = resumed ? 0.0 : s_mouseY - s_lastMouseY;
    s_mouseWheelY = resumed ? 0.0 : s_pendingMouseWheelY;
    s_pendingMouseWheelY = 0.0;

    for (int i = 0; i < 512; ++i) {
        const bool current = glfwGetKey(window, i) == GLFW_PRESS;
        s_keyPressed[i] = !resumed && current && !s_keyDown[i];
        s_keyDown[i] = current;
    }

    for (int i = 0; i < 8; ++i) {
        const bool current = glfwGetMouseButton(window, i) == GLFW_PRESS;
        s_mousePressed[i] = !resumed && current && !s_mouseDown[i];
        s_mouseDown[i] = current;
    }
}

void Input::scrollCallback(
    GLFWwindow* window,
    double xOffset,
    double yOffset)
{
    (void)xOffset;
    if (window == s_window && s_active) {
        s_pendingMouseWheelY += yOffset;
    }
}

void Input::clearState() noexcept
{
    s_keyDown.fill(false);
    s_keyPressed.fill(false);
    s_mouseDown.fill(false);
    s_mousePressed.fill(false);
    s_mouseDeltaX = 0.0;
    s_mouseDeltaY = 0.0;
    s_mouseWheelY = 0.0;
    s_pendingMouseWheelY = 0.0;
    s_active = false;
    s_interactionActive = false;
}

bool Input::isKeyDown(int key)
{
    return key >= 0 && key < static_cast<int>(s_keyDown.size()) ? s_keyDown[key] : false;
}

bool Input::isKeyPressed(int key)
{
    return key >= 0 && key < static_cast<int>(s_keyPressed.size()) ? s_keyPressed[key] : false;
}

bool Input::isMouseButtonDown(int button)
{
    return button >= 0 && button < static_cast<int>(s_mouseDown.size()) ? s_mouseDown[button] : false;
}

bool Input::isMouseButtonPressed(int button)
{
    return button >= 0 && button < static_cast<int>(s_mousePressed.size()) ? s_mousePressed[button] : false;
}

float Input::mouseX() { return static_cast<float>(s_mouseX); }
float Input::mouseY() { return static_cast<float>(s_mouseY); }
float Input::mouseDeltaX() { return static_cast<float>(s_mouseDeltaX); }
float Input::mouseDeltaY() { return static_cast<float>(s_mouseDeltaY); }
float Input::mouseWheelY() { return static_cast<float>(s_mouseWheelY); }
bool Input::isActive() { return s_interactionActive; }
uint64_t Input::interactionEpoch() { return s_interactionEpoch; }

void Input::setMousePosition(double x, double y)
{
    GLFWwindow* window = glfwGetCurrentContext();
    if (!window) {
        return;
    }

    glfwSetCursorPos(window, x, y);
    s_mouseX = x;
    s_mouseY = y;
    s_lastMouseX = x;
    s_lastMouseY = y;
}

} // namespace ku
