#include <chrono>
#include <iostream>
#include <stdexcept>
#include <span>
#include <vector>

#include <KuEngine/Core/Log.h>
#include <KuEngine/Core/Window.h>
#include <KuEngine/RHI/CommandList.h>
#include <KuEngine/RHI/RHIDevice.h>
#include <KuEngine/RHI/RHITexture.h>
#include <KuEngine/RHI/RHIInstance.h>
#include <KuEngine/RHI/SwapChain.h>
#include <KuEngine/RHI/SyncManager.h>
#include <KuEngine/Render/RenderPipeline.h>
#include <KuEngine/UI/UIOverlay.h>

#include <GLFW/glfw3.h>

#include "MclarenPass.h"

namespace {

VkFormat chooseDepthFormat(const ku::RHIDevice& device)
{
    const std::vector<VkFormat> candidates = {
        VK_FORMAT_D32_SFLOAT,
        VK_FORMAT_D32_SFLOAT_S8_UINT,
        VK_FORMAT_D24_UNORM_S8_UINT,
    };

    for (const VkFormat format : candidates) {
        VkFormatProperties props{};
        vkGetPhysicalDeviceFormatProperties(device.physicalDevice(), format, &props);
        if ((props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) != 0) {
            return format;
        }
    }

    throw std::runtime_error("No supported depth format found");
}

void transitionDepthToAttachment(ku::RHIDevice& device, ku::RHITexture& depthTexture)
{
    VkCommandPool pool = device.createCommandPool();
    ku::CommandList cmd(device, pool);
    cmd.begin();
    cmd.imageBarrier(
        depthTexture.image(),
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
        VK_IMAGE_ASPECT_DEPTH_BIT);
    cmd.end();

    VkCommandBuffer buffer = cmd.cmd();
    VkSubmitInfo submit{};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &buffer;
    VK_CHECK(vkQueueSubmit(device.graphicsQueue(), 1, &submit, VK_NULL_HANDLE));
    VK_CHECK(vkQueueWaitIdle(device.graphicsQueue()));

    vkDestroyCommandPool(device.device(), pool, nullptr);
}

} // namespace

int main(int argc, char* argv[])
{
    (void)argc;
    (void)argv;

    try {
        ku::log::init();

        ku::Window window("KuEngine Mclaren", 1280, 720);
        ku::RHIInstance instance("KuEngineMclaren", 1u);
        VkSurfaceKHR surface = instance.createSurface(window.handle());

        {
            ku::RHIDevice device(instance.instance(), surface);
            VkCommandPool commandPool = device.createCommandPool();

            {
                ku::SwapChain swapChain(device, window.handle(), surface);
                ku::SyncManager syncManager(device, 1);
                ku::CommandList commandList(device, commandPool);
                const VkFormat depthFormat = chooseDepthFormat(device);
                ku::UIOverlay uiOverlay(
                    device,
                    window.handle(),
                    instance.instance(),
                    swapChain.imageFormat(),
                    static_cast<uint32_t>(swapChain.imageCount()),
                    depthFormat);

                ku::RenderPipeline renderPipeline;
                ku::MclarenPass& mclarenPass = renderPipeline.addPass<ku::MclarenPass>();
                mclarenPass.setDepthFormat(depthFormat);

                renderPipeline.compile(device);
                renderPipeline.setExecuteInsideRendering(true);
                mclarenPass.onResize(swapChain.width(), swapChain.height());

                std::unique_ptr<ku::RHITexture> depthTexture;
                auto recreateDepthAttachment = [&]() {
                    ku::RHITexture::CreateInfo depthInfo{};
                    depthInfo.width = swapChain.width();
                    depthInfo.height = swapChain.height();
                    depthInfo.format = depthFormat;
                    depthInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
                    depthInfo.aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
                    depthTexture = std::make_unique<ku::RHITexture>(device, depthInfo);
                    transitionDepthToAttachment(device, *depthTexture);
                };
                recreateDepthAttachment();

                std::vector<VkImageLayout> imageLayouts(swapChain.imageCount(), VK_IMAGE_LAYOUT_UNDEFINED);

                bool resizeRequested = false;
                window.onResize([&](int, int) {
                    resizeRequested = true;
                });

                bool dragging = false;
                double lastMouseX = 0.0;
                double lastMouseY = 0.0;

                using Clock = std::chrono::high_resolution_clock;
                auto lastTime = Clock::now();
                float totalTime = 0.0f;

                while (!window.shouldClose()) {
                    window.processEvents();

                    const auto now = Clock::now();
                    const float deltaTime = std::chrono::duration<float>(now - lastTime).count();
                    lastTime = now;
                    totalTime += deltaTime;

                    GLFWwindow* glfwWindow = window.handle();
                    const bool leftDown = glfwGetMouseButton(glfwWindow, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
                    double mouseX = 0.0;
                    double mouseY = 0.0;
                    glfwGetCursorPos(glfwWindow, &mouseX, &mouseY);

                    if (leftDown) {
                        if (!dragging) {
                            dragging = true;
                            lastMouseX = mouseX;
                            lastMouseY = mouseY;
                        } else {
                            const float dx = static_cast<float>(mouseX - lastMouseX);
                            const float dy = static_cast<float>(mouseY - lastMouseY);
                            mclarenPass.addRotation(dx * 0.01f, dy * 0.01f);
                            lastMouseX = mouseX;
                            lastMouseY = mouseY;
                        }
                    } else {
                        dragging = false;
                    }

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
                        mclarenPass.onResize(swapChain.width(), swapChain.height());
                        recreateDepthAttachment();
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
                    colorAttachment.clearValue.color = {{0.05f, 0.05f, 0.08f, 1.0f}};

                    VkRenderingAttachmentInfo depthAttachment{};
                    depthAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
                    depthAttachment.imageView = depthTexture->imageView();
                    depthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
                    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
                    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
                    depthAttachment.clearValue.depthStencil.depth = 1.0f;
                    depthAttachment.clearValue.depthStencil.stencil = 0;

                    VkRenderingInfo renderingInfo{};
                    renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
                    renderingInfo.renderArea.offset = {0, 0};
                    renderingInfo.renderArea.extent = swapChain.extent();
                    renderingInfo.layerCount = 1;
                    renderingInfo.colorAttachmentCount = 1;
                    renderingInfo.pColorAttachments = &colorAttachment;
                    renderingInfo.pDepthAttachment = &depthAttachment;

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
                    syncManager.submit(frameIndex, device.graphicsQueue(), {&buffer, 1});

                    const bool presented =
                        syncManager.present(frameIndex, device.presentQueue(), swapChain.swapChain(), imageIndex);
                    if (!presented || resizeRequested) {
                        device.waitIdle();
                        swapChain.recreate(window.handle(), surface);
                        uiOverlay.onSwapChainRecreated(static_cast<uint32_t>(swapChain.imageCount()));
                        imageLayouts.assign(swapChain.imageCount(), VK_IMAGE_LAYOUT_UNDEFINED);
                        renderPipeline.clearExternalResources();
                        renderPipeline.onResize(swapChain.width(), swapChain.height());
                        recreateDepthAttachment();
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
