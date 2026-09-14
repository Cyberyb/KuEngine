// KuEngine 输入模块：基于窗口事件维护键盘与鼠标状态，向上层提供逐帧输入查询。
#pragma once

#include <GLFW/glfw3.h>
#include <array>
#include <cstdint>

namespace ku {

class Window;

class Input {
public:
    static void attach(GLFWwindow* window);
    static void detach(GLFWwindow* window);
    static void update(GLFWwindow* window, bool active = true);

    [[nodiscard]] static bool isKeyDown(int key);
    [[nodiscard]] static bool isKeyPressed(int key);
    [[nodiscard]] static bool isMouseButtonDown(int button);
    [[nodiscard]] static bool isMouseButtonPressed(int button);
    [[nodiscard]] static float mouseX();
    [[nodiscard]] static float mouseY();
    [[nodiscard]] static float mouseDeltaX();
    [[nodiscard]] static float mouseDeltaY();
    [[nodiscard]] static float mouseWheelY();
    [[nodiscard]] static bool isActive();
    // Changes whenever pointer/key interaction continuity is invalidated
    // (attach/detach, focus loss, minimize, or active resume).
    [[nodiscard]] static uint64_t interactionEpoch();

    static void setMousePosition(double x, double y);

    // 常用键码
    static constexpr int KEY_ESCAPE = GLFW_KEY_ESCAPE;
    static constexpr int KEY_W = GLFW_KEY_W;
    static constexpr int KEY_A = GLFW_KEY_A;
    static constexpr int KEY_S = GLFW_KEY_S;
    static constexpr int KEY_D = GLFW_KEY_D;
    static constexpr int KEY_Q = GLFW_KEY_Q;
    static constexpr int KEY_E = GLFW_KEY_E;
    static constexpr int MOUSE_BUTTON_LEFT = GLFW_MOUSE_BUTTON_LEFT;
    static constexpr int MOUSE_BUTTON_RIGHT = GLFW_MOUSE_BUTTON_RIGHT;

private:
    static void scrollCallback(GLFWwindow* window, double xOffset, double yOffset);
    static void clearState() noexcept;

    static GLFWwindow* s_window;
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
    static double  s_mouseWheelY;
    static double  s_pendingMouseWheelY;
    static bool    s_active;
    static bool    s_interactionActive;
    static uint64_t s_interactionEpoch;

    friend class Window;
};

} // namespace ku
