#include <gtest/gtest.h>

#include "KuEngine/RHI/VulkanValidation.h"

#include <thread>
#include <vector>

namespace {

TEST(VulkanValidation, SelectsOnlyAvailableRequestedFeatures)
{
    constexpr auto notRequested = ku::selectValidationFeatures(false, true, true);
    EXPECT_FALSE(notRequested.requested);
    EXPECT_FALSE(notRequested.validationEnabled);
    EXPECT_FALSE(notRequested.debugUtilsEnabled);

    constexpr auto missingLayer = ku::selectValidationFeatures(true, false, true);
    EXPECT_TRUE(missingLayer.requested);
    EXPECT_FALSE(missingLayer.validationEnabled);
    EXPECT_FALSE(missingLayer.debugUtilsEnabled);

    constexpr auto missingDebugUtils = ku::selectValidationFeatures(true, true, false);
    EXPECT_TRUE(missingDebugUtils.requested);
    EXPECT_TRUE(missingDebugUtils.validationEnabled);
    EXPECT_FALSE(missingDebugUtils.debugUtilsEnabled);

    constexpr auto allAvailable = ku::selectValidationFeatures(true, true, true);
    EXPECT_TRUE(allAvailable.requested);
    EXPECT_TRUE(allAvailable.validationEnabled);
    EXPECT_TRUE(allAvailable.debugUtilsEnabled);
}

TEST(VulkanValidation, ExposesStateAndRoutesCallbackToCounts)
{
    ku::VulkanValidationState state(true);
    EXPECT_TRUE(state.requested());
    EXPECT_FALSE(state.validationEnabled());
    EXPECT_FALSE(state.messageCaptureEnabled());

    state.configureCapabilities(true, true);
    EXPECT_TRUE(state.validationEnabled());
    EXPECT_TRUE(state.debugUtilsEnabled());
    EXPECT_FALSE(state.messageCaptureEnabled());

    state.setMessageCaptureEnabled(true);
    EXPECT_TRUE(state.messageCaptureEnabled());

    const VkDebugUtilsMessengerCreateInfoEXT createInfo =
        ku::makeDebugMessengerCreateInfo(&state);
    EXPECT_EQ(createInfo.sType, VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT);
    EXPECT_EQ(createInfo.pUserData, &state);
    ASSERT_NE(createInfo.pfnUserCallback, nullptr);
    EXPECT_NE(
        createInfo.messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT,
        0u);
    EXPECT_NE(
        createInfo.messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
        0u);

    VkDebugUtilsMessengerCallbackDataEXT callbackData{};
    callbackData.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CALLBACK_DATA_EXT;
    callbackData.pMessageIdName = "unit-test";
    callbackData.pMessage = "controlled validation callback";

    EXPECT_EQ(
        createInfo.pfnUserCallback(
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT,
            VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT,
            &callbackData,
            createInfo.pUserData),
        VK_FALSE);
    EXPECT_EQ(
        createInfo.pfnUserCallback(
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
            VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT,
            nullptr,
            createInfo.pUserData),
        VK_FALSE);

    const ku::ValidationMessageCounts counts = state.counts();
    EXPECT_EQ(counts.warnings, 1u);
    EXPECT_EQ(counts.errors, 1u);

    state.reset();
    const ku::ValidationMessageCounts resetCounts = state.counts();
    EXPECT_EQ(resetCounts.warnings, 0u);
    EXPECT_EQ(resetCounts.errors, 0u);

    state.setMessageCaptureEnabled(false);
    EXPECT_FALSE(state.messageCaptureEnabled());
}

TEST(VulkanValidation, CountsConcurrentRecordingBeforeReset)
{
    ku::VulkanValidationState state(true);
    constexpr int threadCount = 4;
    constexpr int recordsPerThread = 1000;

    std::vector<std::thread> workers;
    workers.reserve(threadCount);
    for (int threadIndex = 0; threadIndex < threadCount; ++threadIndex) {
        workers.emplace_back([&state]() {
            for (int recordIndex = 0; recordIndex < recordsPerThread; ++recordIndex) {
                state.record(VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT);
                state.record(VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT);
            }
        });
    }
    for (std::thread& worker : workers) {
        worker.join();
    }

    const ku::ValidationMessageCounts counts = state.counts();
    EXPECT_EQ(counts.warnings, static_cast<uint64_t>(threadCount * recordsPerThread));
    EXPECT_EQ(counts.errors, static_cast<uint64_t>(threadCount * recordsPerThread));

    state.reset();
    const ku::ValidationMessageCounts resetCounts = state.counts();
    EXPECT_EQ(resetCounts.warnings, 0u);
    EXPECT_EQ(resetCounts.errors, 0u);
}

TEST(VulkanValidation, ExternalTrackerSurvivesValidationStateTeardown)
{
    auto tracker = std::make_shared<ku::ValidationMessageTracker>();
    {
        ku::VulkanValidationState state(true, tracker);
        state.record(VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT);
    }

    EXPECT_EQ(tracker->counts().errors, 1u);
}

} // namespace
