#include "SyncManager.h"
#include "RHIDevice.h"
#include "../Core/Log.h"

namespace ku {

SyncManager::SyncManager(const RHIDevice& device, uint32_t framesInFlight)
    : m_device(&device)
{
    m_frames.resize(framesInFlight);
    for (auto& f : m_frames) {
        VkSemaphoreCreateInfo semInfo{};
        semInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        VK_CHECK(vkCreateSemaphore(device.device(), &semInfo, nullptr, &f.imageAvailable));
        VK_CHECK(vkCreateSemaphore(device.device(), &semInfo, nullptr, &f.renderFinished));
        VK_CHECK(vkCreateFence(device.device(), &fenceInfo, nullptr, &f.inFlight));
    }
    KU_INFO("SyncManager created ({} frames)", framesInFlight);
}

SyncManager::~SyncManager()
{
    auto dev = m_device->device();
    for (auto& f : m_frames) {
        vkDestroySemaphore(dev, f.imageAvailable, nullptr);
        vkDestroySemaphore(dev, f.renderFinished, nullptr);
        vkDestroyFence(dev, f.inFlight, nullptr);
    }
    KU_INFO("SyncManager destroyed");
}

void SyncManager::waitForFrame(uint32_t frame)
{
    vkWaitForFences(m_device->device(), 1, &m_frames[frame].inFlight, VK_TRUE, UINT64_MAX);
    vkResetFences(m_device->device(), 1, &m_frames[frame].inFlight);
}

void SyncManager::submit(uint32_t frame, VkQueue queue, std::span<VkCommandBuffer> cmds)
{
    VkSemaphore waits[] = {m_frames[frame].imageAvailable};
    VkPipelineStageFlags stages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    VkSemaphore signals[] = {m_frames[frame].renderFinished};

    VkSubmitInfo info{};
    info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    info.waitSemaphoreCount = 1;
    info.pWaitSemaphores = waits;
    info.pWaitDstStageMask = stages;
    info.commandBufferCount = static_cast<uint32_t>(cmds.size());
    info.pCommandBuffers = cmds.data();
    info.signalSemaphoreCount = 1;
    info.pSignalSemaphores = signals;

    VK_CHECK(vkQueueSubmit(queue, 1, &info, m_frames[frame].inFlight));
}

bool SyncManager::present(uint32_t frame, VkQueue queue, VkSwapchainKHR swapChain, uint32_t imageIndex)
{
    VkSemaphore waits[] = {m_frames[frame].renderFinished};
    VkSwapchainKHR swaps[] = {swapChain};

    VkPresentInfoKHR info{};
    info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    info.waitSemaphoreCount = 1;
    info.pWaitSemaphores = waits;
    info.swapchainCount = 1;
    info.pSwapchains = swaps;
    info.pImageIndices = &imageIndex;

    VkResult result = vkQueuePresentKHR(queue, &info);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) return false;
    VK_CHECK(result);
    return true;
}

} // namespace ku
