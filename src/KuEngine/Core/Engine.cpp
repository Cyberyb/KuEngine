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

#include <thread>
#include <stdexcept>

namespace ku {

Engine::Engine()
{
    ku::log::init();
    try {
        m_window = std::make_unique<Window>("KuEngine v" KU_VERSION, 1280, 720);
        m_instance = std::make_unique<RHIInstance>("KuEngine", 1u);
        m_surface = m_instance->createSurface(m_window->handle());
        m_device = std::make_unique<RHIDevice>(m_instance->instance(), m_surface);
        m_commandPool = m_device->createCommandPool();
        m_swapChain = std::make_unique<SwapChain>(*m_device, m_window->handle(), m_surface);
        m_syncManager = std::make_unique<SyncManager>(*m_device, FRAMES_IN_FLIGHT);
        m_commandList = std::make_unique<CommandList>(*m_device, m_commandPool);
        m_ui = std::make_unique<UIOverlay>(
            *m_device,
            m_window->handle(),
            m_instance->instance(),
            m_swapChain->imageFormat(),
            static_cast<uint32_t>(m_swapChain->imageCount()));
        m_renderPipeline = std::make_unique<RenderPipeline>();
        m_lastTime = Clock::now();
    } catch (...) {
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

void Engine::run()
{
    KU_INFO("Starting main loop");
    m_running = true;
    mainLoop();
}

void Engine::mainLoop()
{
    auto lastTime = Clock::now();

    while (m_running && m_window && !m_window->shouldClose()) {
        auto now = Clock::now();
        m_deltaTime = std::chrono::duration<float>(now - lastTime).count();
        lastTime = now;
        m_totalTime += m_deltaTime;

        pollEvents();
        render();

        // Keep the placeholder loop from consuming 100% CPU.
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
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
    // Render backend is still under development in this MVP baseline.
}

} // namespace ku
