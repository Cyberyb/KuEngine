#include "Window.h"
#include "Log.h"

#include <stdexcept>

namespace ku {

namespace {
int g_glfwWindowCount = 0;
}

Window::Window(std::string_view title, int width, int height)
    : m_width(width), m_height(height)
{
    if (g_glfwWindowCount == 0) {
        if (!glfwInit()) {
            throw std::runtime_error("Failed to initialize GLFW");
        }
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    m_window = glfwCreateWindow(width, height,
                                std::string(title).c_str(), nullptr, nullptr);
    if (!m_window) {
        if (g_glfwWindowCount == 0) {
            glfwTerminate();
        }
        throw std::runtime_error("Failed to create GLFW window");
    }

    ++g_glfwWindowCount;
    glfwSetWindowUserPointer(m_window, this);
    KU_INFO("Window created: {}x{}", width, height);

    glfwSetFramebufferSizeCallback(m_window, framebufferResizeCallback);
    glfwSetWindowCloseCallback(m_window, windowCloseCallback);
}

Window::~Window()
{
    if (m_window) {
        glfwDestroyWindow(m_window);
        m_window = nullptr;
        --g_glfwWindowCount;
        if (g_glfwWindowCount == 0) {
            glfwTerminate();
        }
        KU_INFO("Window destroyed");
    }
}

void Window::setTitle(std::string_view title)
{
    glfwSetWindowTitle(m_window, std::string(title).c_str());
}

void Window::processEvents()
{
    // Poll events and update input state
    glfwPollEvents();
}

void Window::swapBuffers()
{
    glfwSwapBuffers(m_window);
}

void Window::framebufferResizeCallback(GLFWwindow* window, int width, int height)
{
    auto* win = static_cast<Window*>(glfwGetWindowUserPointer(window));
    if (win) {
        win->m_resized = true;
        if (width == 0 || height == 0) win->m_minimized = true;
        else win->m_minimized = false;
        if (win->m_onResize) win->m_onResize(width, height);
    }
}

void Window::windowCloseCallback(GLFWwindow* window)
{
    auto* win = static_cast<Window*>(glfwGetWindowUserPointer(window));
    if (win && win->m_onClose) win->m_onClose();
}

} // namespace ku
