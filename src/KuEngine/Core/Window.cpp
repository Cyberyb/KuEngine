#include "Window.h"
#include "Input.h"

#include <spdlog/spdlog.h>

namespace ku {

Window::Window(std::string_view title, int width, int height)
    : m_width(width), m_height(height) {
    
    if (!glfwInit()) {
        throw std::runtime_error("Failed to initialize GLFW");
    }

    // Vulkan 需要，不创建 OpenGL 上下文
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    m_window = glfwCreateWindow(width, height, std::string(title).c_str(), nullptr, nullptr);
    if (!m_window) {
        glfwTerminate();
        throw std::runtime_error("Failed to create GLFW window");
    }

    // 设置用户指针，方便回调访问
    glfwSetWindowUserPointer(m_window, this);

    // 设置回调
    glfwSetFramebufferSizeCallback(m_window, framebufferResizeCallback);
    glfwSetWindowCloseCallback(m_window, windowCloseCallback);

    // 设置 raw mouse input（可选）
    if (glfwRawMouseMotionSupported()) {
        glfwSetInputMode(m_window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
    }

    KU_INFO("Window created: {}x{}", width, height);
}

Window::~Window() {
    if (m_window) {
        glfwDestroyWindow(m_window);
    }
    glfwTerminate();
}

void Window::setTitle(std::string_view title) {
    glfwSetWindowTitle(m_window, std::string(title).c_str());
}

void Window::processEvents() {
    glfwPollEvents();

    // 更新 Input 状态
    Input::update(m_window);
}

void Window::swapBuffers() {
    glfwSwapBuffers(m_window);
}

void Window::framebufferResizeCallback(GLFWwindow* window, int width, int height) {
    auto* self = static_cast<Window*>(glfwGetWindowUserPointer(window));
    if (self) {
        self->m_width = width;
        self->m_height = height;
        self->m_resized = true;

        // 检查是否最小化
        self->m_minimized = (width == 0 || height == 0);

        if (self->m_onResize && !self->m_minimized) {
            self->m_onResize(width, height);
        }
    }
}

void Window::windowCloseCallback(GLFWwindow* window) {
    auto* self = static_cast<Window*>(glfwGetWindowUserPointer(window));
    if (self && self->m_onClose) {
        self->m_onClose();
    }
}

} // namespace ku
