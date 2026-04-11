#include "Input.h"

namespace ku {

std::array<bool, 512> Input::s_keyDown{};
std::array<bool, 512> Input::s_keyPressed{};
std::array<bool, 8>   Input::s_mouseDown{};
std::array<bool, 8>   Input::s_mousePressed{};
double Input::s_mouseX = 0;
double Input::s_mouseY = 0;
double Input::s_mouseDeltaX = 0;
double Input::s_mouseDeltaY = 0;
double Input::s_lastMouseX = 0;
double Input::s_lastMouseY = 0;

void Input::update(GLFWwindow* window) {
    // 清除 pressed 状态（每帧重置）
    s_keyPressed = {};
    s_mousePressed = {};

    // 更新 delta
    double x, y;
    glfwGetCursorPos(window, &x, &y);
    s_mouseDeltaX = x - s_lastMouseX;
    s_mouseDeltaY = y - s_lastMouseY;
    s_lastMouseX = s_mouseX;
    s_lastMouseY = s_mouseY;
    s_mouseX = x;
    s_mouseY = y;

    // 更新按键状态
    for (int i = 0; i < 512; i++) {
        bool current = glfwGetKey(window, i) == GLFW_PRESS;
        s_keyPressed[i] = current && !s_keyDown[i];
        s_keyDown[i] = current;
    }

    // 更新鼠标状态
    for (int i = 0; i < 8; i++) {
        bool current = glfwGetMouseButton(window, i) == GLFW_PRESS;
        s_mousePressed[i] = current && !s_mouseDown[i];
        s_mouseDown[i] = current;
    }
}

bool Input::isKeyDown(int key) {
    if (key < 0 || key >= 512) return false;
    return s_keyDown[key];
}

bool Input::isKeyPressed(int key) {
    if (key < 0 || key >= 512) return false;
    return s_keyPressed[key];
}

bool Input::isMouseButtonDown(int button) {
    if (button < 0 || button >= 8) return false;
    return s_mouseDown[button];
}

bool Input::isMouseButtonPressed(int button) {
    if (button < 0 || button >= 8) return false;
    return s_mousePressed[button];
}

float Input::mouseX() { return static_cast<float>(s_mouseX); }
float Input::mouseY() { return static_cast<float>(s_mouseY); }
float Input::mouseDeltaX() { return static_cast<float>(s_mouseDeltaX); }
float Input::mouseDeltaY() { return static_cast<float>(s_mouseDeltaY); }

void Input::setMousePosition(double x, double y) {
    glfwSetCursorPos(glfwGetCurrentContext(), x, y);
    s_mouseX = x;
    s_mouseY = y;
    s_lastMouseX = x;
    s_lastMouseY = y;
}

} // namespace ku
