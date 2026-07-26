// KuEngine 引擎核心模块：统一管理窗口、RHI、渲染管线与主循环等运行时生命周期。
#pragma once

#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>
#include <vulkan/vulkan.h>

#include "../Render/RenderPipeline.h"

#define KU_VERSION "0.1.0"

namespace ku {

class Window;
class RHIInstance;
class RHIDevice;
class SwapChain;
class SyncManager;
class CommandList;
class RHITexture;
class UIOverlay;
namespace log {
void init();
}

struct EngineConfig {
    std::string title = "KuEngine v" KU_VERSION;
    uint32_t width = 1280;
    uint32_t height = 720;
    // First Runtime milestone intentionally keeps the proven single-frame path.
    uint32_t framesInFlight = 1;
    bool showStats = true;
    bool enableDepth = false;
    // VK_FORMAT_UNDEFINED selects the first supported Runtime depth format.
    VkFormat depthFormat = VK_FORMAT_UNDEFINED;
    VkClearColorValue clearColor{{0.08f, 0.09f, 0.12f, 1.0f}};
    VkClearDepthStencilValue clearDepthStencil{1.0f, 0};
    VkAttachmentLoadOp depthLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    VkAttachmentStoreOp depthStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    VkCompareOp depthCompareOp = VK_COMPARE_OP_LESS;
};

class Engine {
public:
    explicit Engine(EngineConfig config = {});
    ~Engine();

    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;

    template<typename T, typename... Args>
    T& addPass(Args&&... args)
    {
        m_pipelineCompiled = false;
        return m_renderPipeline->addPass<T>(std::forward<Args>(args)...);
    }

    void compile();
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
    [[nodiscard]] VkFormat          depthFormat()    const { return m_depthFormat; }

    [[nodiscard]] const EngineConfig& config() const { return m_config; }
    [[nodiscard]] uint32_t currentFrame() const;
    [[nodiscard]] bool     isRunning()    const { return m_running; }
    [[nodiscard]] float   deltaTime()    const { return m_deltaTime; }
    [[nodiscard]] float   totalTime()    const { return m_totalTime; }

    using Clock = std::chrono::high_resolution_clock;

private:
    void mainLoop();
    void pollEvents();
    void render();
    void recreateSwapChain();
    void createDepthAttachment();
    [[nodiscard]] VkFormat resolveDepthFormat(VkFormat requested) const;

    EngineConfig m_config;
    std::unique_ptr<Window>        m_window;
    std::unique_ptr<RHIInstance>   m_instance;
    std::unique_ptr<RHIDevice>     m_device;
    std::unique_ptr<SwapChain>     m_swapChain;
    std::unique_ptr<SyncManager>   m_syncManager;
    std::unique_ptr<CommandList>   m_commandList;
    std::unique_ptr<RHITexture>    m_depthTexture;
    std::unique_ptr<UIOverlay>     m_ui;
    std::unique_ptr<RenderPipeline> m_renderPipeline;

    VkSurfaceKHR  m_surface = VK_NULL_HANDLE;
    VkCommandPool m_commandPool = VK_NULL_HANDLE;
    VkFormat      m_depthFormat = VK_FORMAT_UNDEFINED;
    VkImageLayout m_depthImageLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    bool     m_running = false;
    bool     m_minimized = false;
    bool     m_resizeRequested = false;
    bool     m_pipelineCompiled = false;
    bool     m_depthInitialized = false;
    float    m_deltaTime = 0.0f;
    float    m_totalTime = 0.0f;
    Clock::time_point m_lastTime;
    std::vector<VkImageLayout> m_swapChainImageLayouts;
};

} // namespace ku
