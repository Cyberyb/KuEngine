#include "SyncManager.h"
#include "RHIDevice.h"
#include "../Core/Log.h"
#include <stdexcept>

namespace ku {

SyncManager::SyncManager(const RHIDevice& device, uint32_t framesInFlight)
    : m_device(&device)
{
    if (framesInFlight < 1 || framesInFlight > 2) throw std::invalid_argument("SyncManager supports one or two frame slots");
    m_frames.resize(framesInFlight);
    try {
    for (auto& f : m_frames) {
        VkSemaphoreCreateInfo semInfo{};
        semInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        VK_CHECK(vkCreateSemaphore(device.device(), &semInfo, nullptr, &f.imageAvailable));
        VK_CHECK(vkCreateFence(device.device(), &fenceInfo, nullptr, &f.inFlight));
    }
    } catch (...) {
        for (auto& f : m_frames) {
            if (f.imageAvailable) vkDestroySemaphore(device.device(), f.imageAvailable, nullptr);
            if (f.inFlight) vkDestroyFence(device.device(), f.inFlight, nullptr);
        }
        throw;
    }
    KU_INFO("SyncManager created ({} frames)", framesInFlight);
}

SyncManager::~SyncManager()
{
    auto dev = m_device->device();
    for (auto& f : m_frames) {
        vkDestroySemaphore(dev, f.imageAvailable, nullptr);
        vkDestroyFence(dev, f.inFlight, nullptr);
    }
    for (auto semaphore : m_presentSemaphores) vkDestroySemaphore(dev, semaphore, nullptr);
    KU_INFO("SyncManager destroyed");
}

void SyncManager::recreatePresentSemaphores(uint32_t imageCount)
{
    std::vector<VkSemaphore> candidate(imageCount, VK_NULL_HANDLE);
    try {
        VkSemaphoreCreateInfo info{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
        for (auto& semaphore : candidate)
            VK_CHECK(vkCreateSemaphore(m_device->device(), &info, nullptr, &semaphore));
    } catch (...) {
        for (auto semaphore : candidate) if (semaphore) vkDestroySemaphore(m_device->device(), semaphore, nullptr);
        throw;
    }
    for (auto semaphore : m_presentSemaphores) vkDestroySemaphore(m_device->device(), semaphore, nullptr);
    m_presentSemaphores.swap(candidate);
}

void SyncManager::waitForFrame(uint32_t frame)
{
    VK_CHECK(vkWaitForFences(m_device->device(), 1, &m_frames.at(frame).inFlight, VK_TRUE, UINT64_MAX));
}

void SyncManager::submit(uint32_t frame, VkQueue queue, std::span<VkCommandBuffer> cmds)
{
    VK_CHECK(vkResetFences(m_device->device(), 1, &m_frames.at(frame).inFlight));

    VkSemaphore waits[] = {m_frames[frame].imageAvailable};
    // The acquired swapchain image is an imported graph resource. Its first use
    // is selected by the compiled graph, so the semaphore wait must not assume
    // that the image is first consumed as a color attachment.
    VkPipelineStageFlags stages[] = {VK_PIPELINE_STAGE_ALL_COMMANDS_BIT};
    VkSemaphore signals[] = {m_presentSemaphores.at(m_currentImage)};

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
    VkSemaphore waits[] = {m_presentSemaphores.at(imageIndex)};
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
