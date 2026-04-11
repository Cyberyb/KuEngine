#include "Window.h"
#include "Input.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_events.h>
#include <spdlog/spdlog.h>

namespace ku {

Window::Window(std::string_view title, int width, int height)
    : m_width(width), m_height(height) {
    
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        throw std::runtime_error("Failed to initialize SDL: " + std::string(SDL_GetError()));
    }

    m_window = SDL_CreateWindow(
        std::string(title).c_str(),
        width, height,
        SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_SHOWN
    );

    if (!m_window) {
        SDL_Quit();
        throw std::runtime_error("Failed to create window: " + std::string(SDL_GetError()));
    }

    KU_INFO("Window created: {}x{}", width, height);
}

Window::~Window() {
    if (m_window) {
        SDL_DestroyWindow(m_window);
    }
    SDL_Quit();
}

void Window::setTitle(std::string_view title) {
    SDL_SetWindowTitle(m_window, std::string(title).c_str());
}

void Window::processEvents() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
        case SDL_EVENT_QUIT:
            m_shouldClose = true;
            if (m_onClose) m_onClose();
            break;

        case SDL_EVENT_WINDOW_RESIZED:
            m_width = event.window.data1;
            m_height = event.window.data2;
            m_resized = true;
            if (m_onResize) m_onResize(m_width, m_height);
            break;

        case SDL_EVENT_WINDOW_MINIMIZED:
            m_minimized = true;
            break;

        case SDL_EVENT_WINDOW_RESTORED:
            m_minimized = false;
            m_resized = true;
            if (m_onResize) m_onResize(m_width, m_height);
            break;

        case SDL_EVENT_KEY_DOWN:
            Input::update();
            break;
        }
    }
}

void Window::swap() {
    SDL_Vulkan_SetSwapchainMode ? SDL_Vulkan_SetSwapchainMode(nullptr, SDL_VULKAN_SWAPCHAIN_MODE_MAILBOX) : (void)0;
}

} // namespace ku
