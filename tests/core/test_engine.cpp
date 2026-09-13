// KuEngine core tests
#include <gtest/gtest.h>
#include <KuEngine/Core/Engine.h>

#include <type_traits>

namespace {

class UpdateCountingPass final : public ku::RenderPass {
public:
    [[nodiscard]] std::string_view name() const override { return "UpdateCounting"; }

    void update(const ku::FrameData&) override
    {
        ++updateCount;
    }

    int updateCount = 0;
};

class ExpectedStatisticsPass final : public ku::RenderPass {
public:
    ExpectedStatisticsPass(uint64_t draws, uint64_t vertices)
        : m_expected{draws, vertices}
    {
    }

    [[nodiscard]] std::string_view name() const override
    {
        return "ExpectedStatistics";
    }

    [[nodiscard]] std::optional<ku::CommandListStatistics>
    expectedFrameStatistics() const override
    {
        return m_expected;
    }

private:
    ku::CommandListStatistics m_expected{};
};

} // namespace

TEST(KuEngineTest, VersionMacro)
{
    EXPECT_STREQ(KU_VERSION, "0.1.0");
}

TEST(KuEngineTest, RuntimeClockIsMonotonic)
{
    static_assert(ku::Engine::Clock::is_steady);
    EXPECT_TRUE(ku::Engine::Clock::is_steady);
}

TEST(KuEngineTest, RuntimeConfigUsesSingleFrameBaseline)
{
    const ku::EngineConfig config{};

    EXPECT_EQ(config.width, 1280u);
    EXPECT_EQ(config.height, 720u);
    EXPECT_EQ(config.framesInFlight, 1u);
    EXPECT_TRUE(config.showStats);
    EXPECT_FALSE(config.enableDepth);
    EXPECT_EQ(config.depthFormat, VK_FORMAT_UNDEFINED);
    EXPECT_EQ(config.depthLoadOp, VK_ATTACHMENT_LOAD_OP_CLEAR);
    EXPECT_EQ(config.depthStoreOp, VK_ATTACHMENT_STORE_OP_DONT_CARE);
    EXPECT_EQ(config.depthCompareOp, VK_COMPARE_OP_LESS);
    EXPECT_FLOAT_EQ(config.clearDepthStencil.depth, 1.0f);
}

TEST(KuEngineTest, RuntimeRejectsDepthLoadWithoutPersistence)
{
    ku::EngineConfig config{};
    config.enableDepth = true;
    config.depthLoadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    config.depthStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

    EXPECT_THROW(
        {
            ku::Engine engine(config);
        },
        std::invalid_argument);
}

TEST(KuEngineTest, RuntimeRejectsUnsupportedMultiFrameConfiguration)
{
    ku::EngineConfig config{};
    config.framesInFlight = 2;

    EXPECT_THROW(
        {
            ku::Engine engine(config);
        },
        std::invalid_argument);
}

TEST(KuEngineTest, RenderPipelineUpdatesOnlyEnabledPasses)
{
    ku::RenderPipeline pipeline;
    auto& pass = pipeline.addPass<UpdateCountingPass>();

    const ku::FrameData frame{};
    pipeline.update(frame);
    EXPECT_EQ(pass.updateCount, 1);

    pass.setEnabled(false);
    pipeline.update(frame);
    EXPECT_EQ(pass.updateCount, 1);
}

TEST(KuEngineTest, RenderPipelineAggregatesOnlyEnabledPassExpectations)
{
    ku::RenderPipeline pipeline;
    auto& first = pipeline.addPass<ExpectedStatisticsPass>(1, 3);
    pipeline.addPass<ExpectedStatisticsPass>(2, 12);

    const auto allEnabled = pipeline.expectedFrameStatistics();
    ASSERT_TRUE(allEnabled.has_value());
    EXPECT_EQ(allEnabled->drawCalls, 3u);
    EXPECT_EQ(allEnabled->submittedVertices, 15u);

    first.setEnabled(false);
    const auto firstDisabled = pipeline.expectedFrameStatistics();
    ASSERT_TRUE(firstDisabled.has_value());
    EXPECT_EQ(firstDisabled->drawCalls, 2u);
    EXPECT_EQ(firstDisabled->submittedVertices, 12u);

    pipeline.addPass<UpdateCountingPass>();
    EXPECT_FALSE(pipeline.expectedFrameStatistics().has_value());
}

TEST(KuEngineTest, RunProgressCountsOnlySubmittedFrames)
{
    ku::EngineRunOptions options{};
    options.submittedFrameLimit = 4;
    options.resize = ku::EngineResizeRequest{2, 960, 540};
    ku::EngineRunResult result{};

    const ku::EngineRunDecision skipped =
        ku::advanceEngineRun(options, false, result);
    EXPECT_EQ(result.submittedFrames, 0u);
    EXPECT_FALSE(skipped.requestResize);
    EXPECT_FALSE(skipped.stop);

    const ku::EngineRunDecision first =
        ku::advanceEngineRun(options, true, result);
    EXPECT_EQ(result.submittedFrames, 1u);
    EXPECT_FALSE(first.requestResize);
    EXPECT_FALSE(first.stop);

    const ku::EngineRunDecision second =
        ku::advanceEngineRun(options, true, result);
    EXPECT_EQ(result.submittedFrames, 2u);
    EXPECT_TRUE(second.requestResize);
    EXPECT_FALSE(second.stop);

    const ku::EngineRunDecision third =
        ku::advanceEngineRun(options, true, result);
    EXPECT_FALSE(third.requestResize);
    EXPECT_FALSE(third.stop);

    const ku::EngineRunDecision fourth =
        ku::advanceEngineRun(options, true, result);
    EXPECT_TRUE(fourth.stop);
    EXPECT_TRUE(result.reachedFrameLimit);
}
