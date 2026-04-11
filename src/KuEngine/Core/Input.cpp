#include "Input.h"

#include <GLFW/glfw3.h>

namespace ku {

// static member definitions
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

void Input::update(GLFWwindow* window)
{
    s_lastMouseX = s_mouseX;
    s_lastMouseY = s_mouseY;
    glfwGetCursorPos(window, &s_mouseX, &s_mouseY);
    s_mouseDeltaX = s_mouseX - s_lastMouseX;
    s_mouseDeltaY = s_mouseY - s_lastMouseY;

    for (int i = 0; i < 512; ++i) {
        const bool current = glfwGetKey(window, i) == GLFW_PRESS;
        s_keyPressed[i] = current && !s_keyDown[i];
        s_keyDown[i] = current;
    }

    for (int i = 0; i < 8; ++i) {
        const bool current = glfwGetMouseButton(window, i) == GLFW_PRESS;
        s_mousePressed[i] = current && !s_mouseDown[i];
        s_mouseDown[i] = current;
    }
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
