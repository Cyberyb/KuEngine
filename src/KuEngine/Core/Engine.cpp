#include "Engine.h"
#include "Window.h"
#include "Input.h"
#include "Log.h"

#include "../RHI/RHIInstance.h"
#include "../RHI/RHIDevice.h"
#include "../RHI/SwapChain.h"
#include "../RHI/SyncManager.h"
#include "../RHI/CommandList.h"
#include "../UI/UIOverlay.h"
#include "../Render/RenderPipeline.h"

#include <span>
#include <stdexcept>
#include <thread>
#include <utility>

namespace ku {

EngineRunDecision advanceEngineRun(
    const EngineRunOptions& options,
    bool frameSubmitted,
    EngineRunResult& result) noexcept
{
    EngineRunDecision decision{};
    if (!frameSubmitted) {
        return decision;
    }

    ++result.submittedFrames;
    if (options.resize
        && !result.resizeRequested
        && result.submittedFrames == options.resize->afterSubmittedFrame) {
        result.resizeRequested = true;
        decision.requestResize = true;
    }
    if (options.submittedFrameLimit > 0
        && result.submittedFrames >= options.submittedFrameLimit) {
        result.reachedFrameLimit = true;
        decision.stop = true;
    }
    return decision;
}

Engine::Engine(
    EngineConfig config,
    std::shared_ptr<ValidationMessageTracker> validationMessages)
    : m_config(std::move(config))
{
    if (m_config.width == 0 || m_config.height == 0) {
        throw std::invalid_argument("Engine window dimensions must be greater than zero");
    }
    if (m_config.framesInFlight != 1) {
        throw std::invalid_argument(
            "Public Runtime currently supports exactly one frame in flight because command buffers, "
            "Graph resources, and Pass-owned dynamic resources are not yet replicated per frame");
    }
    ku::log::init();
    try {
        m_window = std::make_unique<Window>(
            m_config.title,
            static_cast<int>(m_config.width),
            static_cast<int>(m_config.height));
        Input::attach(m_window->handle());
        m_instance = std::make_unique<RHIInstance>(
            "KuEngine",
            1u,
            std::move(validationMessages));
        m_surface = m_instance->createSurface(m_window->handle());
        m_device = std::make_unique<RHIDevice>(m_instance->instance(), m_surface);
        m_commandPool = m_device->createCommandPool();
        m_swapChain = std::make_unique<SwapChain>(*m_device, m_window->handle(), m_surface);
        m_syncManager = std::make_unique<SyncManager>(*m_device, m_config.framesInFlight);
        m_commandList = std::make_unique<CommandList>(*m_device, m_commandPool);
        if (m_config.enableDepth) {
            m_depthFormat = resolveDepthFormat(m_config.depthFormat);
        }
        m_ui = std::make_unique<UIOverlay>(
            *m_device,
            m_window->handle(),
            m_instance->instance(),
            m_swapChain->imageFormat(),
            static_cast<uint32_t>(m_swapChain->imageCount()));
        m_renderPipeline = std::make_unique<RenderPipeline>();
        m_swapChainImageLayouts.assign(
            m_swapChain->imageCount(),
            VK_IMAGE_LAYOUT_UNDEFINED);
        m_window->onResize([this](int width, int height) {
            m_minimized = width == 0 || height == 0;
            m_resizeRequested = !m_minimized;
        });
        m_lastTime = Clock::now();
    } catch (...) {
        m_ui.reset();
        m_commandList.reset();
        m_syncManager.reset();
        m_swapChain.reset();

        if (m_commandPool != VK_NULL_HANDLE && m_device) {
            vkDestroyCommandPool(m_device->device(), m_commandPool, nullptr);
            m_commandPool = VK_NULL_HANDLE;
        }

        m_device.reset();
        if (m_surface != VK_NULL_HANDLE && m_instance) {
            vkDestroySurfaceKHR(m_instance->instance(), m_surface, nullptr);
            m_surface = VK_NULL_HANDLE;
        }
        if (m_window) {
            Input::detach(m_window->handle());
        }
        throw;
    }

    KU_INFO("Engine created (KuEngine v{})", KU_VERSION);
}

Engine::~Engine()
{
    if (m_device) {
        m_device->waitIdle();
    }

    m_renderPipeline.reset();
    m_ui.reset();
    m_commandList.reset();
    m_syncManager.reset();
    m_swapChain.reset();

    if (m_commandPool != VK_NULL_HANDLE && m_device) {
        vkDestroyCommandPool(m_device->device(), m_commandPool, nullptr);
        m_commandPool = VK_NULL_HANDLE;
    }

    m_device.reset();

    if (m_surface != VK_NULL_HANDLE && m_instance) {
        vkDestroySurfaceKHR(m_instance->instance(), m_surface, nullptr);
        m_surface = VK_NULL_HANDLE;
    }

    m_instance.reset();
    if (m_window) {
        Input::detach(m_window->handle());
    }
    m_window.reset();
    KU_INFO("Engine destroyed");
}

void Engine::compile()
{
    if (!m_renderPipeline || !m_device) {
        throw std::runtime_error("Engine Runtime is not initialized");
    }

    m_pipelineCompiled = false;
    const RenderContext context{
        *m_device,
        m_swapChain->imageFormat(),
        m_depthFormat,
        m_swapChain->extent(),
        m_config.framesInFlight,
        m_config.depthCompareOp,
        m_config.clearColor,
        m_config.clearDepthStencil,
        m_device->properties(),
        m_device->features(),
        m_device->features13(),
    };
    m_renderPipeline->compile(context);
    m_pipelineCompiled = true;
}

EngineRunResult Engine::run(const EngineRunOptions& options)
{
    if (!m_pipelineCompiled) {
        compile();
    }

    if (options.sidebarExpanded.has_value()) {
        m_ui->setSidebarExpanded(*options.sidebarExpanded);
    }

    KU_INFO("Starting main loop");
    m_running = true;
    m_lastTime = Clock::now();
    return mainLoop(options);
}

EngineRunResult Engine::mainLoop(const EngineRunOptions& options)
{
    EngineRunResult result{};
    uint64_t resizeGenerationAtRequest = 0;
    while (m_running && m_window && !m_window->shouldClose()) {
        pollEvents();

        const auto now = Clock::now();
        m_deltaTime = std::chrono::duration<float>(now - m_lastTime).count();
        m_lastTime = now;
        m_totalTime += m_deltaTime;

        if (m_window->isMinimized() || m_minimized) {
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
            continue;
        }

        const bool frameSubmitted = render();
        if (frameSubmitted) {
            if (!result.firstSubmittedViewerLayout.has_value()) {
                result.firstSubmittedViewerLayout = m_viewerLayout;
            }
            result.finalSubmittedViewerLayout = m_viewerLayout;
        }
        if (result.resizeRequested
            && !result.resizeCompleted
            && m_swapChainGeneration > resizeGenerationAtRequest) {
            result.resizeCompleted = true;
            KU_INFO(
                "KUENGINE_SMOKE_RESIZE_COMPLETE generation={} size={}x{}",
                m_swapChainGeneration,
                m_swapChain->width(),
                m_swapChain->height());
        }

        const EngineRunDecision decision =
            advanceEngineRun(options, frameSubmitted, result);
        if (decision.requestResize) {
            resizeGenerationAtRequest = m_swapChainGeneration;
            glfwSetWindowSize(
                m_window->handle(),
                static_cast<int>(options.resize->width),
                static_cast<int>(options.resize->height));
            KU_INFO(
                "KUENGINE_SMOKE_RESIZE after_frame={} size={}x{}",
                result.submittedFrames,
                options.resize->width,
                options.resize->height);
        }

        if (decision.stop) {
            m_running = false;
        }
    }

    if (m_device) {
        m_device->waitIdle();
    }
    completePendingFrameStatistics();
    result.completedStatistics = m_frameStatistics.completedFrame();
    if (m_renderPipeline) {
        result.expectedStatistics =
            m_renderPipeline->expectedFrameStatistics();
    }
    return result;
}

void Engine::pollEvents()
{
    if (!m_window) {
        return;
    }

    m_window->processEvents();
    Input::update(
        m_window->handle(),
        m_window->isFocused()
            && !m_window->isMinimized()
            && !m_minimized);
}

bool Engine::render()
{
    if (m_resizeRequested || m_window->wasResized()) {
        recreateSwapChain();
        return false;
    }

    const uint32_t frameIndex = m_syncManager->currentFrame();
    m_syncManager->waitForFrame(frameIndex);
    completePendingFrameStatistics();

    const uint32_t imageIndex =
        m_swapChain->acquireNextImage(m_syncManager->frameSync(frameIndex).imageAvailable);
    if (imageIndex == UINT32_MAX) {
        recreateSwapChain();
        return false;
    }
    m_syncManager->setCurrentImage(imageIndex);
    const auto cpuRenderStart = Clock::now();

    FrameData frameData{};
    frameData.frameIndex = frameIndex;
    frameData.imageIndex = imageIndex;
    frameData.deltaTime = m_deltaTime;
    frameData.totalTime = m_totalTime;

    const SidebarFrameLayout sidebarFrame = m_ui->describeSidebarFrame(
        static_cast<float>(m_window->logicalWidth()),
        static_cast<float>(m_window->logicalHeight()),
        m_config.showStats);
    m_viewerLayout = calculateViewerLayout(ViewerLayoutInput{
        .windowLogicalWidth =
            static_cast<double>(m_window->logicalWidth()),
        .windowLogicalHeight =
            static_cast<double>(m_window->logicalHeight()),
        .framebufferExtent = {
            m_swapChain->extent().width,
            m_swapChain->extent().height,
        },
        .sidebarLogicalWidth =
            static_cast<double>(sidebarFrame.reservedSceneLogicalWidth),
        .reserveSidebar =
            sidebarFrame.mode == SidebarLayoutMode::Expanded,
        .uiOverlayLogical = {
            static_cast<double>(sidebarFrame.window.x),
            static_cast<double>(sidebarFrame.window.y),
            static_cast<double>(sidebarFrame.window.width),
            static_cast<double>(sidebarFrame.window.height),
        },
    });
    frameData.viewerLayout = m_viewerLayout;

    m_ui->newFrame();
    m_renderPipeline->update(frameData);

    UIFrameStatistics uiStats{};
    uiStats.fps =
        m_deltaTime > 0.0f ? (1.0f / m_deltaTime) : 0.0f;
    uiStats.frameTimeMilliseconds = m_deltaTime * 1000.0f;
    uiStats.completedFrame = m_frameStatistics.completedFrame();
    uiStats.gpuStatusBeforeFirstCompletedFrame =
        m_commandList->gpuTimingSupported()
        ? GpuTimeStatus::Waiting
        : GpuTimeStatus::Unsupported;
    m_ui->drawSidebar(
        sidebarFrame,
        uiStats,
        [this]() { m_renderPipeline->drawPassUIContent(); },
        [this]() { m_renderPipeline->drawRenderGraphUIContent(); });

    VkClearValue colorClear{};
    colorClear.color = m_config.clearColor;
    m_renderPipeline->bindExternalImage({
        .resourceName = runtime_resource::swapChainColor,
        .image = m_swapChain->images()[imageIndex],
        .imageView = m_swapChain->imageViews()[imageIndex],
        .extent = m_swapChain->extent(),
        .format = m_swapChain->imageFormat(),
        .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .initialState = {
            VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_ACCESS_2_NONE,
            m_swapChainImageLayouts[imageIndex]},
        .aspect = VK_IMAGE_ASPECT_COLOR_BIT,
        .clearValue = colorClear,
        .defaultLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .defaultStoreOp = VK_ATTACHMENT_STORE_OP_STORE,
        .finalState = {
            VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT,
            VK_ACCESS_2_NONE,
            VK_IMAGE_LAYOUT_PRESENT_SRC_KHR},
        .contentsValid = false,
    });

    // External resource descriptors are validated before command recording so
    // a format/extent/aspect mismatch cannot leave a partially recorded frame.
    m_commandList->begin();

    m_renderPipeline->execute(*m_commandList, frameData);
    m_renderPipeline->executeOverlay(
        *m_commandList,
        [this, imageIndex](VkCommandBuffer cmd) {
            m_ui->render(
                cmd,
                m_swapChain->imageViews()[imageIndex],
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        });
    m_renderPipeline->finalizeExternalImages(*m_commandList);

    m_swapChainImageLayouts[imageIndex] = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    m_commandList->end();

    VkCommandBuffer buffer = m_commandList->cmd();
    m_syncManager->submit(
        frameIndex,
        m_device->graphicsQueue(),
        std::span<VkCommandBuffer>(&buffer, 1));
    const float cpuRenderTimeMilliseconds =
        std::chrono::duration<float, std::milli>(
        Clock::now() - cpuRenderStart).count();
    if (!m_frameStatistics.recordSubmittedFrame(
            cpuRenderTimeMilliseconds,
            m_commandList->statistics())) {
        throw std::logic_error(
            "A submitted frame statistics sample is still pending completion");
    }
    const bool presented = m_syncManager->present(
        frameIndex,
        m_device->presentQueue(),
        m_swapChain->swapChain(),
        imageIndex);
    m_syncManager->incrementFrame();

    if (!presented || m_resizeRequested || m_window->wasResized()) {
        recreateSwapChain();
    }
    return true;
}

void Engine::completePendingFrameStatistics()
{
    if (!m_commandList || !m_frameStatistics.hasPendingFrame()) {
        return;
    }

    const GpuTimeSample gpuTime = m_commandList->collectGpuTime();
    if (!m_frameStatistics.completePendingFrame(gpuTime)) {
        throw std::logic_error(
            "Unable to publish pending submitted frame statistics");
    }
}

void Engine::recreateSwapChain()
{
    if (!m_window || !m_device || !m_swapChain) {
        return;
    }

    int width = 0;
    int height = 0;
    glfwGetFramebufferSize(m_window->handle(), &width, &height);
    while ((width == 0 || height == 0) && !m_window->shouldClose()) {
        m_minimized = true;
        glfwWaitEvents();
        glfwGetFramebufferSize(m_window->handle(), &width, &height);
    }

    if (m_window->shouldClose()) {
        return;
    }

    m_minimized = false;
    const VkFormat previousFormat = m_swapChain->imageFormat();
    m_swapChain->recreate(m_window->handle(), m_surface);
    const bool formatChanged = swapchainFormatRequiresPipelineRecompile(
        previousFormat, m_swapChain->imageFormat());
    m_swapChainImageLayouts.assign(
        m_swapChain->imageCount(),
        VK_IMAGE_LAYOUT_UNDEFINED);
    m_renderPipeline->clearExternalResources();
    if (formatChanged) {
        KU_INFO(
            "SwapChain format changed from {} to {}; recompiling render and UI pipelines",
            static_cast<int>(previousFormat),
            static_cast<int>(m_swapChain->imageFormat()));
        compile();
        const bool sidebarExpanded = m_ui->sidebarExpanded();
        const bool compactStatsVisible = m_ui->compactStatsVisible();
        m_ui.reset();
        m_ui = std::make_unique<UIOverlay>(
            *m_device,
            m_window->handle(),
            m_instance->instance(),
            m_swapChain->imageFormat(),
            static_cast<uint32_t>(m_swapChain->imageCount()));
        m_ui->setSidebarExpanded(sidebarExpanded);
        m_ui->setCompactStatsVisible(compactStatsVisible);
    } else {
        m_ui->onSwapChainRecreated(
            static_cast<uint32_t>(m_swapChain->imageCount()));
        m_renderPipeline->onResize(m_swapChain->width(), m_swapChain->height());
    }

    m_window->resetResizedFlag();
    m_resizeRequested = false;
    ++m_swapChainGeneration;

    KU_INFO(
        "Engine Runtime recreated SwapChain: {}x{} ({} images)",
        m_swapChain->width(),
        m_swapChain->height(),
        m_swapChain->imageCount());
}

VkFormat Engine::resolveDepthFormat(VkFormat requested) const
{
    if (!m_device) {
        throw std::runtime_error(
            "Depth format selection requires an initialized Runtime device");
    }

    const auto isSupported = [this](VkFormat format) {
        VkFormatProperties properties{};
        vkGetPhysicalDeviceFormatProperties(
            m_device->physicalDevice(),
            format,
            &properties);
        return (properties.optimalTilingFeatures
                & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) != 0;
    };

    if (requested != VK_FORMAT_UNDEFINED) {
        if (!isSupported(requested)) {
            throw std::runtime_error("Requested Runtime depth format is not supported");
        }
        return requested;
    }

    constexpr VkFormat candidates[] = {
        VK_FORMAT_D32_SFLOAT,
        VK_FORMAT_D32_SFLOAT_S8_UINT,
        VK_FORMAT_D24_UNORM_S8_UINT,
    };
    for (const VkFormat format : candidates) {
        if (isSupported(format)) {
            return format;
        }
    }

    throw std::runtime_error("No supported Runtime depth format found");
}

uint32_t Engine::currentFrame() const
{
    return m_syncManager ? m_syncManager->currentFrame() : 0;
}

} // namespace ku
