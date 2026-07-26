// KuEngine core tests
#include <gtest/gtest.h>
#include <KuEngine/Core/Engine.h>

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

} // namespace

TEST(KuEngineTest, VersionMacro)
{
    EXPECT_STREQ(KU_VERSION, "0.1.0");
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
    EXPECT_FLOAT_EQ(config.clearDepthStencil.depth, 1.0f);
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
