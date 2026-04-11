#pragma once

#define SDL_MAIN_USE_CALLBACKS
#include <SDL3/SDL.h>
#include <string>
#include <string_view>
#include <functional>

namespace ku {

class Window {
public:
    Window(std::string_view title, int width, int height);
    ~Window();

    [[nodiscard]] SDL_Window* sdlWindow() { return m_window; }
    [[nodiscard]] int width() const { return m_width; }
    [[nodiscard]] int height() const { return m_height; }
    [[nodiscard]] bool shouldClose() const { return m_shouldClose; }
    [[nodiscard]] bool wasResized() const { return m_resized; }
    [[nodiscard]] bool isMinimized() const { return m_minimized; }

    void resetResizedFlag() { m_resized = false; }
    void setTitle(std::string_view title);
    void processEvents();

    using ResizeCallback = std::function<void(int, int)>;
    void onResize(ResizeCallback cb) { m_onResize = std::move(cb); }

    using CloseCallback = std::function<void()>;
    void onClose(CloseCallback cb) { m_onClose = std::move(cb); }

private:
    SDL_Window* m_window = nullptr;
    int         m_width = 0;
    int         m_height = 0;
    bool        m_shouldClose = false;
    bool        m_resized = false;
    bool        m_minimized = false;

    ResizeCallback m_onResize;
    CloseCallback  m_onClose;
};

} // namespace ku
