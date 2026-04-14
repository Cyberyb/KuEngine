// KuEngine - Triangle App

#include <iostream>
#include <chrono>
#include <span>
#include <vector>

#include <KuEngine/Core/Window.h>
#include <KuEngine/Core/Log.h>
#include <KuEngine/RHI/RHIInstance.h>
#include <KuEngine/RHI/RHIDevice.h>
#include <KuEngine/RHI/SwapChain.h>
#include <KuEngine/RHI/SyncManager.h>
#include <KuEngine/RHI/CommandList.h>
#include <KuEngine/Render/RenderPipeline.h>
#include <KuEngine/UI/UIOverlay.h>

#include "TrianglePass.h"

int main(int argc, char* argv[])
{
    (void)argc;
    (void)argv;

    try {
        ku::log::init();

        ku::Window window("KuEngine Triangle", 1280, 720);
        ku::RHIInstance instance("KuEngineTriangle", 1u);
        VkSurfaceKHR surface = instance.createSurface(window.handle());

        {
            ku::RHIDevice device(instance.instance(), surface);
            VkCommandPool commandPool = device.createCommandPool();

            {
                ku::SwapChain swapChain(device, window.handle(), surface);
                ku::SyncManager syncManager(device, 1);
                ku::CommandList commandList(device, commandPool);
                ku::UIOverlay uiOverlay(
                    device,
                    window.handle(),
                    instance.instance(),
                    swapChain.imageFormat(),
                    static_cast<uint32_t>(swapChain.imageCount()));

                ku::RenderPipeline renderPipeline;
                renderPipeline.addPass<ku::TrianglePass>();
                renderPipeline.compile(device);
                renderPipeline.setExecuteInsideRendering(true);

                std::vector<VkImageLayout> imageLayouts(
                    swapChain.imageCount(),
                    VK_IMAGE_LAYOUT_UNDEFINED);

                bool resizeRequested = false;
                window.onResize([&](int, int) {
                    resizeRequested = true;
                });

                using Clock = std::chrono::high_resolution_clock;
                auto lastTime = Clock::now();
                float totalTime = 0.0f;

                while (!window.shouldClose()) {
                    window.processEvents();

                    const auto now = Clock::now();
                    const float deltaTime = std::chrono::duration<float>(now - lastTime).count();
                    lastTime = now;
                    totalTime += deltaTime;

                    uiOverlay.newFrame();

                    const float fps = deltaTime > 0.0f ? (1.0f / deltaTime) : 0.0f;
                    uiOverlay.drawFPSPanel(fps, deltaTime);
                    renderPipeline.drawUI();

                    const uint32_t frameIndex = syncManager.currentFrame();
                    syncManager.waitForFrame(frameIndex);

                    const uint32_t imageIndex =
                        swapChain.acquireNextImage(syncManager.frameSync(frameIndex).imageAvailable);
                    if (imageIndex == UINT32_MAX) {
                        device.waitIdle();
                        swapChain.recreate(window.handle(), surface);
                        uiOverlay.onSwapChainRecreated(static_cast<uint32_t>(swapChain.imageCount()));
                        imageLayouts.assign(swapChain.imageCount(), VK_IMAGE_LAYOUT_UNDEFINED);
                        renderPipeline.clearExternalResources();
                        resizeRequested = false;
                        continue;
                    }

                    commandList.begin();

                    commandList.imageBarrier(
                        swapChain.images()[imageIndex],
                        imageLayouts[imageIndex],
                        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
                    imageLayouts[imageIndex] = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                    renderPipeline.bindExternalImage(
                        "SwapChainColor",
                        swapChain.images()[imageIndex],
                        imageLayouts[imageIndex]);

                    VkViewport viewport{};
                    viewport.x = 0.0f;
                    viewport.y = 0.0f;
                    viewport.width = static_cast<float>(swapChain.width());
                    viewport.height = static_cast<float>(swapChain.height());
                    viewport.minDepth = 0.0f;
                    viewport.maxDepth = 1.0f;
                    vkCmdSetViewport(commandList, 0, 1, &viewport);

                    VkRect2D scissor{};
                    scissor.offset = {0, 0};
                    scissor.extent = swapChain.extent();
                    vkCmdSetScissor(commandList, 0, 1, &scissor);

                    VkRenderingAttachmentInfo colorAttachment{};
                    colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
                    colorAttachment.imageView = swapChain.imageViews()[imageIndex];
                    colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
                    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
                    colorAttachment.clearValue.color = {{0.08f, 0.09f, 0.12f, 1.0f}};

                    VkRenderingInfo renderingInfo{};
                    renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
                    renderingInfo.renderArea.offset = {0, 0};
                    renderingInfo.renderArea.extent = swapChain.extent();
                    renderingInfo.layerCount = 1;
                    renderingInfo.colorAttachmentCount = 1;
                    renderingInfo.pColorAttachments = &colorAttachment;

                    vkCmdBeginRendering(commandList, &renderingInfo);

                    ku::FrameData frameData{};
                    frameData.frameIndex = frameIndex;
                    frameData.imageIndex = imageIndex;
                    frameData.deltaTime = deltaTime;
                    frameData.totalTime = totalTime;
                    renderPipeline.execute(commandList, frameData);
                    uiOverlay.render(
                        commandList.cmd(),
                        swapChain.imageViews()[imageIndex],
                        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

                    vkCmdEndRendering(commandList);

                    commandList.imageBarrier(
                        swapChain.images()[imageIndex],
                        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                        VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                        VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);
                    imageLayouts[imageIndex] = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

                    commandList.end();

                    VkCommandBuffer buffer = commandList.cmd();
                    syncManager.submit(frameIndex, device.graphicsQueue(), std::span<VkCommandBuffer>(&buffer, 1));

                    const bool presented =
                        syncManager.present(frameIndex, device.presentQueue(), swapChain.swapChain(), imageIndex);
                    if (!presented || resizeRequested) {
                        device.waitIdle();
                        swapChain.recreate(window.handle(), surface);
                        uiOverlay.onSwapChainRecreated(static_cast<uint32_t>(swapChain.imageCount()));
                        imageLayouts.assign(swapChain.imageCount(), VK_IMAGE_LAYOUT_UNDEFINED);
                        renderPipeline.clearExternalResources();
                        renderPipeline.onResize(swapChain.width(), swapChain.height());
                        resizeRequested = false;
                    }

                    syncManager.incrementFrame();
                }

                device.waitIdle();
            }

            if (commandPool != VK_NULL_HANDLE) {
                vkDestroyCommandPool(device.device(), commandPool, nullptr);
            }
        }

        vkDestroySurfaceKHR(instance.instance(), surface, nullptr);
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << "\n";
        return 1;
    }
}
