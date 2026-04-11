#pragma once

#include <array>

namespace ku {

class Input {
public:
    static void update();

    [[nodiscard]] static bool isKeyDown(int keycode);
    [[nodiscard]] static bool isKeyPressed(int keycode);
    [[nodiscard]] static bool isMouseButtonDown(int button);
    [[nodiscard]] static bool isMouseButtonPressed(int button);
    [[nodiscard]] static float mouseX();
    [[nodiscard]] static float mouseY();
    [[nodiscard]] static float mouseDeltaX();
    [[nodiscard]] static float mouseDeltaY();
    [[nodiscard]] static float mouseWheelY();

    static void setMousePosition(float x, float y);

private:
    static void processEvent(const void* sdlEvent);

    static std::array<bool, 512>      s_keyDown;
    static std::array<bool, 512>      s_keyPressed;
    static std::array<bool, 8>        s_mouseDown;
    static std::array<bool, 8>        s_mousePressed;
    static float                      s_mouseX;
    static float                      s_mouseY;
    static float                      s_mouseDeltaX;
    static float                      s_mouseDeltaY;
    static float                      s_mouseWheelY;

    friend class Window;
};

} // namespace ku
