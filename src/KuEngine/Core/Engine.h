#pragma once

#include <memory>
#include <chrono>
#include <cstdint>
#include <vulkan/vulkan.h>

#define KU_VERSION "0.1.0"

namespace ku {

class Window;
class RHIInstance;
class RHIDevice;
class SwapChain;
class SyncManager;
class CommandList;
class UIOverlay;
class RenderPipeline;

namespace log {
void init();
}

class Engine {
public:
    Engine();
    ~Engine();

    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;

    void run();
    void quit() { m_running = false; }

    [[nodiscard]] Window&           window()         const { return *m_window; }
    [[nodiscard]] RHIInstance&     instance()       const { return *m_instance; }
    [[nodiscard]] RHIDevice&        device()         const { return *m_device; }
    [[nodiscard]] SwapChain&        swapChain()      const { return *m_swapChain; }
    [[nodiscard]] SyncManager&      syncManager()    const { return *m_syncManager; }
    [[nodiscard]] CommandList&      commandList()    const { return *m_commandList; }
    [[nodiscard]] UIOverlay&        ui()             const { return *m_ui; }
    [[nodiscard]] RenderPipeline&   renderPipeline() const { return *m_renderPipeline; }
    [[nodiscard]] VkSurfaceKHR      surface()        const { return m_surface; }
    [[nodiscard]] VkCommandPool     commandPool()    const { return m_commandPool; }

    [[nodiscard]] uint32_t currentFrame() const { return m_currentFrame; }
    [[nodiscard]] bool     isRunning()    const { return m_running; }
    [[nodiscard]] float   deltaTime()    const { return m_deltaTime; }
    [[nodiscard]] float   totalTime()    const { return m_totalTime; }

    using Clock = std::chrono::high_resolution_clock;

private:
    void mainLoop();
    void pollEvents();
    void render();

    std::unique_ptr<Window>        m_window;
    std::unique_ptr<RHIInstance>   m_instance;
    std::unique_ptr<RHIDevice>     m_device;
    std::unique_ptr<SwapChain>     m_swapChain;
    std::unique_ptr<SyncManager>   m_syncManager;
    std::unique_ptr<CommandList>   m_commandList;
    std::unique_ptr<UIOverlay>     m_ui;
    std::unique_ptr<RenderPipeline> m_renderPipeline;

    VkSurfaceKHR  m_surface = VK_NULL_HANDLE;
    VkCommandPool m_commandPool = VK_NULL_HANDLE;

    bool     m_running = false;
    bool     m_minimized = false;
    uint32_t m_currentFrame = 0;
    float    m_deltaTime = 0.0f;
    float    m_totalTime = 0.0f;
    Clock::time_point m_lastTime;

    static constexpr uint32_t FRAMES_IN_FLIGHT = 2;
};

} // namespace ku
