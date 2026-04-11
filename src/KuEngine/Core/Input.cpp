#include "Input.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_events.h>
#include <array>

namespace ku {

std::array<bool, 512> Input::s_keyDown{};
std::array<bool, 512> Input::s_keyPressed{};
std::array<bool, 8> Input::s_mouseDown{};
std::array<bool, 8> Input::s_mousePressed{};
float Input::s_mouseX = 0;
float Input::s_mouseY = 0;
float Input::s_mouseDeltaX = 0;
float Input::s_mouseDeltaY = 0;
float Input::s_mouseWheelY = 0;

void Input::update() {
    s_keyPressed = {};
    s_mousePressed = {};
    s_mouseDeltaX = 0;
    s_mouseDeltaY = 0;
    s_mouseWheelY = 0;
}

bool Input::isKeyDown(int keycode) {
    if (keycode < 0 || keycode >= 512) return false;
    return s_keyDown[keycode];
}

bool Input::isKeyPressed(int keycode) {
    if (keycode < 0 || keycode >= 512) return false;
    return s_keyPressed[keycode];
}

bool Input::isMouseButtonDown(int button) {
    if (button < 0 || button >= 8) return false;
    return s_mouseDown[button];
}

bool Input::isMouseButtonPressed(int button) {
    if (button < 0 || button >= 8) return false;
    return s_mousePressed[button];
}

float Input::mouseX() { return s_mouseX; }
float Input::mouseY() { return s_mouseY; }
float Input::mouseDeltaX() { return s_mouseDeltaX; }
float Input::mouseDeltaY() { return s_mouseDeltaY; }
float Input::mouseWheelY() { return s_mouseWheelY; }

void Input::setMousePosition(float x, float y) {
    s_mouseX = x;
    s_mouseY = y;
}

} // namespace ku
