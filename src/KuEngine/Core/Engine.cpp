#include "Engine.h"
#include "Window.h"
#include "Input.h"
#include "Log.h"

#include "../RHI/RHIInstance.h"
#include "../RHI/RHIDevice.h"
#include "../RHI/SwapChain.h"
#include "../RHI/SyncManager.h"
#include "../RHI/CommandList.h"
#include "../RHI/RHITexture.h"
#include "../UI/UIOverlay.h"
#include "../Render/RenderPipeline.h"

#include <span>
#include <stdexcept>
#include <thread>
#include <utility>

namespace ku {

Engine::Engine(EngineConfig config)
    : m_config(std::move(config))
{
    if (m_config.width == 0 || m_config.height == 0) {
        throw std::invalid_argument("Engine window dimensions must be greater than zero");
    }
    if (m_config.framesInFlight != 1) {
        throw std::invalid_argument(
            "The first public Runtime implementation currently supports exactly one frame in flight");
    }

    ku::log::init();
    try {
        m_window = std::make_unique<Window>(
            m_config.title,
            static_cast<int>(m_config.width),
            static_cast<int>(m_config.height));
        m_instance = std::make_unique<RHIInstance>("KuEngine", 1u);
        m_surface = m_instance->createSurface(m_window->handle());
        m_device = std::make_unique<RHIDevice>(m_instance->instance(), m_surface);
        m_commandPool = m_device->createCommandPool();
        m_swapChain = std::make_unique<SwapChain>(*m_device, m_window->handle(), m_surface);
        m_syncManager = std::make_unique<SyncManager>(*m_device, m_config.framesInFlight);
        m_commandList = std::make_unique<CommandList>(*m_device, m_commandPool);
        if (m_config.enableDepth) {
            m_depthFormat = resolveDepthFormat(m_config.depthFormat);
            createDepthAttachment();
        }
        m_ui = std::make_unique<UIOverlay>(
            *m_device,
            m_window->handle(),
            m_instance->instance(),
            m_swapChain->imageFormat(),
            static_cast<uint32_t>(m_swapChain->imageCount()),
            m_depthFormat);
        m_renderPipeline = std::make_unique<RenderPipeline>();
        m_renderPipeline->setExecuteInsideRendering(true);
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
        m_depthTexture.reset();
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
    m_depthTexture.reset();
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
    m_window.reset();
    KU_INFO("Engine destroyed");
}

void Engine::compile()
{
    if (!m_renderPipeline || !m_device) {
        throw std::runtime_error("Engine Runtime is not initialized");
    }

    m_renderPipeline->compile(*m_device);
    m_renderPipeline->onResize(m_swapChain->width(), m_swapChain->height());
    m_pipelineCompiled = true;
}

void Engine::run()
{
    if (!m_pipelineCompiled) {
        compile();
    }

    KU_INFO("Starting main loop");
    m_running = true;
    m_lastTime = Clock::now();
    mainLoop();
}

void Engine::mainLoop()
{
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

        render();
    }

    if (m_device) {
        m_device->waitIdle();
    }
}

void Engine::pollEvents()
{
    if (!m_window) {
        return;
    }

    m_window->processEvents();
    Input::update(m_window->handle());
}

void Engine::render()
{
    if (m_resizeRequested || m_window->wasResized()) {
        recreateSwapChain();
        return;
    }

    const uint32_t frameIndex = m_syncManager->currentFrame();
    m_syncManager->waitForFrame(frameIndex);

    const uint32_t imageIndex =
        m_swapChain->acquireNextImage(m_syncManager->frameSync(frameIndex).imageAvailable);
    if (imageIndex == UINT32_MAX) {
        recreateSwapChain();
        return;
    }
    m_syncManager->setCurrentImage(imageIndex);

    FrameData frameData{};
    frameData.frameIndex = frameIndex;
    frameData.imageIndex = imageIndex;
    frameData.deltaTime = m_deltaTime;
    frameData.totalTime = m_totalTime;

    m_ui->newFrame();
    m_renderPipeline->update(frameData);

    const float fps = m_deltaTime > 0.0f ? (1.0f / m_deltaTime) : 0.0f;
    if (m_config.showStats) {
        m_ui->drawFPSPanel(fps, m_deltaTime);
    }
    m_renderPipeline->drawUI();

    m_commandList->begin();

    m_commandList->imageBarrier(
        m_swapChain->images()[imageIndex],
        m_swapChainImageLayouts[imageIndex],
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
    m_swapChainImageLayouts[imageIndex] = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    m_renderPipeline->bindExternalImage(
        "SwapChainColor",
        m_swapChain->images()[imageIndex],
        m_swapChainImageLayouts[imageIndex]);

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(m_swapChain->width());
    viewport.height = static_cast<float>(m_swapChain->height());
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(*m_commandList, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = m_swapChain->extent();
    vkCmdSetScissor(*m_commandList, 0, 1, &scissor);

    VkRenderingAttachmentInfo colorAttachment{};
    colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    colorAttachment.imageView = m_swapChain->imageViews()[imageIndex];
    colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.clearValue.color = m_config.clearColor;

    VkRenderingAttachmentInfo depthAttachment{};
    if (m_depthTexture) {
        depthAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        depthAttachment.imageView = m_depthTexture->imageView();
        depthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
        depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachment.clearValue.depthStencil = m_config.clearDepthStencil;
    }

    VkRenderingInfo renderingInfo{};
    renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderingInfo.renderArea.offset = {0, 0};
    renderingInfo.renderArea.extent = m_swapChain->extent();
    renderingInfo.layerCount = 1;
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachments = &colorAttachment;
    renderingInfo.pDepthAttachment = m_depthTexture ? &depthAttachment : nullptr;

    vkCmdBeginRendering(*m_commandList, &renderingInfo);

    m_renderPipeline->execute(*m_commandList, frameData);
    m_ui->render(
        m_commandList->cmd(),
        m_swapChain->imageViews()[imageIndex],
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    vkCmdEndRendering(*m_commandList);

    m_commandList->imageBarrier(
        m_swapChain->images()[imageIndex],
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);
    m_swapChainImageLayouts[imageIndex] = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    m_commandList->end();

    VkCommandBuffer buffer = m_commandList->cmd();
    m_syncManager->submit(
        frameIndex,
        m_device->graphicsQueue(),
        std::span<VkCommandBuffer>(&buffer, 1));

    const bool presented = m_syncManager->present(
        frameIndex,
        m_device->presentQueue(),
        m_swapChain->swapChain(),
        imageIndex);
    m_syncManager->incrementFrame();

    if (!presented || m_resizeRequested || m_window->wasResized()) {
        recreateSwapChain();
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
    m_swapChain->recreate(m_window->handle(), m_surface);
    if (m_config.enableDepth) {
        createDepthAttachment();
    }
    m_ui->onSwapChainRecreated(static_cast<uint32_t>(m_swapChain->imageCount()));
    m_swapChainImageLayouts.assign(
        m_swapChain->imageCount(),
        VK_IMAGE_LAYOUT_UNDEFINED);
    m_renderPipeline->clearExternalResources();
    m_renderPipeline->onResize(m_swapChain->width(), m_swapChain->height());

    m_window->resetResizedFlag();
    m_resizeRequested = false;

    KU_INFO(
        "Engine Runtime recreated SwapChain: {}x{} ({} images)",
        m_swapChain->width(),
        m_swapChain->height(),
        m_swapChain->imageCount());
}

void Engine::createDepthAttachment()
{
    if (!m_config.enableDepth || m_depthFormat == VK_FORMAT_UNDEFINED) {
        m_depthTexture.reset();
        return;
    }
    if (!m_device || !m_swapChain || !m_commandList) {
        throw std::runtime_error(
            "Depth attachment requires an initialized Runtime device and command list");
    }

    RHITexture::CreateInfo depthInfo{};
    depthInfo.width = m_swapChain->width();
    depthInfo.height = m_swapChain->height();
    depthInfo.format = m_depthFormat;
    depthInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    depthInfo.aspect = VK_IMAGE_ASPECT_DEPTH_BIT;

    m_depthTexture = std::make_unique<RHITexture>(*m_device, depthInfo);

    m_commandList->begin();
    m_commandList->imageBarrier(
        m_depthTexture->image(),
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT
            | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
        VK_IMAGE_ASPECT_DEPTH_BIT);
    m_commandList->end();

    const VkCommandBuffer buffer = m_commandList->cmd();
    VkSubmitInfo submit{};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &buffer;
    VK_CHECK(vkQueueSubmit(m_device->graphicsQueue(), 1, &submit, VK_NULL_HANDLE));
    VK_CHECK(vkQueueWaitIdle(m_device->graphicsQueue()));

    KU_INFO(
        "Engine Runtime created depth attachment: {}x{} format={}",
        depthInfo.width,
        depthInfo.height,
        static_cast<int>(m_depthFormat));
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
