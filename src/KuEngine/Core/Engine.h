// KuEngine 引擎核心模块：统一管理窗口、RHI、渲染管线与主循环等运行时生命周期。
#pragma once

#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>
#include <vulkan/vulkan.h>

#include "FrameStatistics.h"
#include "ViewerLayout.h"
#include "../Render/RenderPipeline.h"

#define KU_VERSION "0.1.0"

namespace ku {

class Window;
class RHIInstance;
class RHIDevice;
class SwapChain;
class SyncManager;
class CommandList;
class UIOverlay;
class ValidationMessageTracker;
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
    VkCompareOp depthCompareOp = VK_COMPARE_OP_LESS;
};

struct EngineResizeRequest {
    uint64_t afterSubmittedFrame = 0;
    uint32_t width = 0;
    uint32_t height = 0;
};

struct EngineRunOptions {
    // Zero preserves the normal interactive, unlimited main loop.
    uint64_t submittedFrameLimit = 0;
    std::optional<EngineResizeRequest> resize;
    // Primarily used by bounded viewer-layout checks. Unset keeps UI defaults.
    std::optional<bool> sidebarExpanded;
};

struct EngineRunResult {
    uint64_t submittedFrames = 0;
    bool reachedFrameLimit = false;
    bool resizeRequested = false;
    bool resizeCompleted = false;
    std::optional<CompletedFrameStatistics> completedStatistics;
    std::optional<CommandListStatistics> expectedStatistics;
    std::optional<ViewerLayout> firstSubmittedViewerLayout;
    std::optional<ViewerLayout> finalSubmittedViewerLayout;
};

struct EngineRunDecision {
    bool requestResize = false;
    bool stop = false;
};

[[nodiscard]] EngineRunDecision advanceEngineRun(
    const EngineRunOptions& options,
    bool frameSubmitted,
    EngineRunResult& result) noexcept;
[[nodiscard]] constexpr bool swapchainFormatRequiresPipelineRecompile(
    VkFormat previous,
    VkFormat current) noexcept
{
    return previous != current;
}

class Engine {
public:
    explicit Engine(
        EngineConfig config = {},
        std::shared_ptr<ValidationMessageTracker> validationMessages = {});
    ~Engine();

    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;

    template<typename T, typename... Args>
    T& addPass(Args&&... args)
    {
        m_pipelineCompiled = false;
        return m_renderPipeline->addPass<T>(std::forward<Args>(args)...);
    }

    template<typename Parameters>
    CallbackRenderPass<Parameters>& addCallbackPass(
        CallbackPassDesc<Parameters> desc)
    {
        m_pipelineCompiled = false;
        return m_renderPipeline->addCallbackPass(std::move(desc));
    }

    void compile();
    [[nodiscard]] EngineRunResult run(const EngineRunOptions& options = {});
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
    [[nodiscard]] const ViewerLayout& viewerLayout() const
    {
        return m_viewerLayout;
    }

    using Clock = std::chrono::steady_clock;

private:
    [[nodiscard]] EngineRunResult mainLoop(const EngineRunOptions& options);
    void pollEvents();
    [[nodiscard]] bool render();
    void completePendingFrameStatistics();
    void recreateSwapChain();
    [[nodiscard]] VkFormat resolveDepthFormat(VkFormat requested) const;

    EngineConfig m_config;
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
    VkFormat      m_depthFormat = VK_FORMAT_UNDEFINED;

    bool     m_running = false;
    bool     m_minimized = false;
    bool     m_resizeRequested = false;
    bool     m_pipelineCompiled = false;
    uint64_t m_swapChainGeneration = 0;
    float    m_deltaTime = 0.0f;
    float    m_totalTime = 0.0f;
    Clock::time_point m_lastTime;
    CompletedFrameStatisticsTracker m_frameStatistics;
    ViewerLayout m_viewerLayout;
    std::vector<VkImageLayout> m_swapChainImageLayouts;
};

} // namespace ku
