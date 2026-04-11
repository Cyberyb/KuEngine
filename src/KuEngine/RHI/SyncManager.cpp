#include "SyncManager.h"
#include "RHIDevice.h"

namespace ku {

SyncManager::SyncManager(const RHIDevice& device, uint32_t framesInFlight)
    : m_device(&device) {
    m_frames.resize(framesInFlight);

    for (auto& frame : m_frames) {
        VkSemaphoreCreateInfo semaphoreInfo{};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        VK_CHECK(vkCreateSemaphore(device.device(), &semaphoreInfo, nullptr, &frame.imageAvailable));
        VK_CHECK(vkCreateSemaphore(device.device(), &semaphoreInfo, nullptr, &frame.renderFinished));

        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        VK_CHECK(vkCreateFence(device.device(), &fenceInfo, nullptr, &frame.inFlight));
    }
}

SyncManager::~SyncManager() {
    for (auto& frame : m_frames) {
        vkDestroyFence(m_device->device(), frame.inFlight, nullptr);
        vkDestroySemaphore(m_device->device(), frame.renderFinished, nullptr);
        vkDestroySemaphore(m_device->device(), frame.imageAvailable, nullptr);
    }
}

void SyncManager::waitForFrame(uint32_t frame) {
    vkWaitForFences(m_device->device(), 1, &m_frames[frame].inFlight, VK_TRUE, UINT64_MAX);
    vkResetFences(m_device->device(), 1, &m_frames[frame].inFlight);
}

void SyncManager::submit(uint32_t frame, VkQueue queue, std::span<VkCommandBuffer> cmds) {
    VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = &m_frames[frame].imageAvailable;
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = static_cast<uint32_t>(cmds.size());
    submitInfo.pCommandBuffers = cmds.data();
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = &m_frames[frame].renderFinished;

    VK_CHECK(vkQueueSubmit(queue, 1, &submitInfo, m_frames[frame].inFlight));
}

bool SyncManager::present(uint32_t frame, VkQueue queue, VkSwapchainKHR swapChain, uint32_t imageIndex) {
    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &m_frames[frame].renderFinished;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &swapChain;
    presentInfo.pImageIndices = &imageIndex;

    VkResult result = vkQueuePresentKHR(queue, &presentInfo);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        return false; // 需要重建
    }
    VK_CHECK(result);
    return true;
}

} // namespace ku
