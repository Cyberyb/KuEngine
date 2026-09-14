// KuEngine 窗口模块：封装 GLFW 窗口生命周期、尺寸变化与原生窗口访问能力。
#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <string>
#include <string_view>
#include <functional>

namespace ku {

class Window {
public:
    Window(std::string_view title, int width, int height);
    ~Window();

    [[nodiscard]] GLFWwindow* handle() { return m_window; }
    [[nodiscard]] const GLFWwindow* handle() const { return m_window; }
    // width/height are framebuffer pixels and remain the resize/Swapchain contract.
    [[nodiscard]] int width() const { return m_width; }
    [[nodiscard]] int height() const { return m_height; }
    [[nodiscard]] int logicalWidth() const { return m_logicalWidth; }
    [[nodiscard]] int logicalHeight() const { return m_logicalHeight; }
    [[nodiscard]] bool shouldClose() const { return glfwWindowShouldClose(m_window) != 0; }
    [[nodiscard]] bool wasResized() const { return m_resized; }
    [[nodiscard]] bool isMinimized() const { return m_minimized; }
    [[nodiscard]] bool isFocused() const { return m_focused; }

    void resetResizedFlag() { m_resized = false; }
    void setTitle(std::string_view title);
    void processEvents();
    void swapBuffers();

    using ResizeCallback = std::function<void(int, int)>;
    void onResize(ResizeCallback cb) { m_onResize = std::move(cb); }

    using CloseCallback = std::function<void()>;
    void onClose(CloseCallback cb) { m_onClose = std::move(cb); }

private:
    static void framebufferResizeCallback(GLFWwindow* window, int width, int height);
    static void windowSizeCallback(GLFWwindow* window, int width, int height);
    static void windowFocusCallback(GLFWwindow* window, int focused);
    static void windowCloseCallback(GLFWwindow* window);

    GLFWwindow* m_window = nullptr;
    int m_width = 0;
    int m_height = 0;
    int m_logicalWidth = 0;
    int m_logicalHeight = 0;
    bool m_resized = false;
    bool m_minimized = false;
    bool m_focused = true;

    ResizeCallback m_onResize;
    CloseCallback  m_onClose;
};

} // namespace ku
