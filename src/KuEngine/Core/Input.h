#pragma once

#include <GLFW/glfw3.h>
#include <array>

namespace ku {

class Window;

class Input {
public:
    static void update(GLFWwindow* window);

    [[nodiscard]] static bool isKeyDown(int key);
    [[nodiscard]] static bool isKeyPressed(int key);
    [[nodiscard]] static bool isMouseButtonDown(int button);
    [[nodiscard]] static bool isMouseButtonPressed(int button);
    [[nodiscard]] static float mouseX();
    [[nodiscard]] static float mouseY();
    [[nodiscard]] static float mouseDeltaX();
    [[nodiscard]] static float mouseDeltaY();

    static void setMousePosition(double x, double y);

    // 常用键码
    static constexpr int KEY_ESCAPE = GLFW_KEY_ESCAPE;
    static constexpr int KEY_W = GLFW_KEY_W;
    static constexpr int KEY_A = GLFW_KEY_A;
    static constexpr int KEY_S = GLFW_KEY_S;
    static constexpr int KEY_D = GLFW_KEY_D;
    static constexpr int MOUSE_BUTTON_LEFT = GLFW_MOUSE_BUTTON_LEFT;
    static constexpr int MOUSE_BUTTON_RIGHT = GLFW_MOUSE_BUTTON_RIGHT;

private:
    static std::array<bool, 512> s_keyDown;
    static std::array<bool, 512> s_keyPressed;
    static std::array<bool, 8>   s_mouseDown;
    static std::array<bool, 8>   s_mousePressed;
    static double  s_mouseX;
    static double  s_mouseY;
    static double  s_mouseDeltaX;
    static double  s_mouseDeltaY;
    static double  s_lastMouseX;
    static double  s_lastMouseY;

    friend class Window;
};

} // namespace ku
