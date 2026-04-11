#include "Engine.h"
#include "Window.h"
#include "Input.h"
#include "Log.h"
#include "RHI/RHIInstance.h"
#include "RHI/RHIDevice.h"
#include "RHI/SwapChain.h"
#include "RHI/SyncManager.h"
#include "RHI/CommandList.h"
#include "UI/UIOverlay.h"
#include "RenderPipeline.h"
#include "examples/triangle/TrianglePass.h"

#include <spdlog/spdlog.h>

namespace ku {

Engine::Engine() {
    log::init();
    KU_INFO("=== KuEngine v{} ===", KU_VERSION);
    KU_INFO("Initializing...");

    // 1. 窗口
    m_window = std::make_unique<Window>("KuEngine v" KU_VERSION, 1280, 720);
    KU_INFO("Window created (1280x720)");

    // 2. Vulkan 实例
    m_instance = std::make_unique<RHIInstance>("KuEngine", 0);
    KU_INFO("Vulkan instance created, validation: {}", m_instance->validationEnabled());

    // 3. Surface
    m_surface = m_instance->createSurface(m_window->sdlWindow());
    KU_INFO("Surface created");

    // 4. 逻辑设备
    m_device = std::make_unique<RHIDevice>(m_instance->instance(), m_surface);
    KU_INFO("Device created: {}", m_device->properties().deviceName);

    // 5. 命令池
    m_commandPool = m_device->createCommandPool();

    // 6. 交换链
    m_swapChain = std::make_unique<SwapChain>(*m_device, m_window->sdlWindow(), m_surface);
    KU_INFO("SwapChain created ({}x{})", m_swapChain->width(), m_swapChain->height());

    // 7. 同步
    m_syncManager = std::make_unique<SyncManager>(*m_device, FRAMES_IN_FLIGHT);

    // 8. 命令缓冲
    m_commandList = std::make_unique<CommandList>(*m_device, m_commandPool);

    // 9. UI
    m_ui = std::make_unique<UIOverlay>(*m_device, m_swapChain->imageFormat());

    // 10. 渲染管线
    m_renderPipeline = std::make_unique<RenderPipeline>();
    m_renderPipeline->addPass<TrianglePass>();
    m_renderPipeline->compile(*m_device);
    KU_INFO("Render pipeline compiled ({} passes)", m_renderPipeline->passCount());

    // 事件回调
    m_window->onResize([this](int w, int h) {
        KU_INFO("Window resized to {}x{}", w, h);
        m_device->waitIdle();
        m_swapChain->recreate(m_window->sdlWindow(), m_surface);
        m_renderPipeline->onResize(w, h);
    });

    m_window->onClose([this]() {
        m_device->waitIdle();
        quit();
    });

    m_running = true;
    m_lastTime = Clock::now();
    KU_INFO("Engine initialized successfully!");
}

Engine::~Engine() {
    if (m_device) {
        m_device->waitIdle();
    }
    m_renderPipeline.reset();
    m_ui.reset();
    m_commandList.reset();
    m_syncManager.reset();
    m_swapChain.reset();
    m_device.reset();
    m_instance.reset();
    KU_INFO("Engine destroyed");
}

void Engine::run() {
    KU_INFO("Entering main loop...");
    mainLoop();
}

void Engine::mainLoop() {
    while (m_running && !m_window->shouldClose()) {
        pollEvents();

        auto now = Clock::now();
        m_deltaTime = std::chrono::duration<float>(now - m_lastTime).count();
        m_lastTime = now;
        m_totalTime += m_deltaTime;

        if (m_window->isMinimized()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        render();
    }

    if (m_device) {
        m_device->waitIdle();
    }
}

void Engine::pollEvents() {
    m_window->processEvents();
    Input::update();
}

void Engine::render() {
    m_syncManager->waitForFrame(m_currentFrame);

    uint32_t imageIndex = m_swapChain->acquireNextImage(
        m_syncManager->frameSync(m_currentFrame).imageAvailable);

    m_commandList->begin();

    // Dynamic Rendering
    VkRenderingInfo renderingInfo{};
    renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderingInfo.renderArea = {{0, 0}, m_swapChain->extent()};
    renderingInfo.layerCount = 1;
    renderingInfo.colorAttachmentCount = 1;

    VkRenderingAttachmentInfo colorAttachment{};
    colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    colorAttachment.imageView = m_swapChain->imageViews()[imageIndex];
    colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.clearValue.color = {{0.1f, 0.1f, 0.15f, 1.0f}};
    renderingInfo.pColorAttachments = &colorAttachment;

    vkCmdBeginRendering(*m_commandList, &renderingInfo);

    FrameData frame{};
    frame.frameIndex = m_currentFrame;
    frame.imageIndex = imageIndex;
    frame.deltaTime = m_deltaTime;
    frame.totalTime = m_totalTime;
    m_renderPipeline->execute(*m_commandList, frame);

    vkCmdEndRendering(*m_commandList);

    // UI
    m_ui->newFrame();
    m_ui->drawFPSPanel(m_deltaTime > 0 ? 1.0f / m_deltaTime : 0.0f, m_deltaTime);
    m_ui->render(*m_commandList, m_swapChain->imageViews()[imageIndex],
                 VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    m_commandList->end();

    m_syncManager->submit(m_currentFrame, m_device->graphicsQueue(),
        std::span<VkCommandBuffer>{ &(*m_commandList), 1 });

    bool needsRebuild = !m_syncManager->present(
        m_currentFrame, m_device->presentQueue(),
        m_swapChain->swapChain(), imageIndex);

    if (needsRebuild) {
        m_device->waitIdle();
        m_swapChain->recreate(m_window->sdlWindow(), m_surface);
    }

    if (!m_window->wasResized()) {
        m_syncManager->incrementFrame();
    }
}

} // namespace ku
