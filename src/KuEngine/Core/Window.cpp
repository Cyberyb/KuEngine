#include "Window.h"
#include "Log.h"
#include "RuntimeError.h"

#include <algorithm>
#include <stdexcept>

namespace ku {

namespace {
int g_glfwWindowCount = 0;
}

Window::Window(std::string_view title, int width, int height)
    : m_width(width),
      m_height(height),
      m_logicalWidth(width),
      m_logicalHeight(height)
{
    if (g_glfwWindowCount == 0) {
        if (!glfwInit()) {
            throw RuntimeUnavailableError("Failed to initialize GLFW");
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
        throw RuntimeUnavailableError("Failed to create GLFW window");
    }

    ++g_glfwWindowCount;
    glfwSetWindowUserPointer(m_window, this);
    glfwGetWindowSize(m_window, &m_logicalWidth, &m_logicalHeight);
    glfwGetFramebufferSize(m_window, &m_width, &m_height);
    m_minimized = m_width == 0 || m_height == 0;
    m_focused = glfwGetWindowAttrib(m_window, GLFW_FOCUSED) == GLFW_TRUE;
    KU_INFO(
        "Window created: logical={}x{} framebuffer={}x{}",
        m_logicalWidth,
        m_logicalHeight,
        m_width,
        m_height);

    glfwSetFramebufferSizeCallback(m_window, framebufferResizeCallback);
    glfwSetWindowSizeCallback(m_window, windowSizeCallback);
    glfwSetWindowFocusCallback(m_window, windowFocusCallback);
    glfwSetWindowCloseCallback(m_window, windowCloseCallback);
}

void Window::windowSizeCallback(GLFWwindow* window, int width, int height)
{
    auto* win = static_cast<Window*>(glfwGetWindowUserPointer(window));
    if (win) {
        win->m_logicalWidth = std::max(width, 0);
        win->m_logicalHeight = std::max(height, 0);
    }
}

void Window::windowFocusCallback(GLFWwindow* window, int focused)
{
    auto* win = static_cast<Window*>(glfwGetWindowUserPointer(window));
    if (win) {
        win->m_focused = focused == GLFW_TRUE;
    }
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
        win->m_width = width;
        win->m_height = height;
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
