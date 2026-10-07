#include <gtest/gtest.h>

#include <KuEngine/Core/Engine.h>
#include <KuEngine/Render/ForwardDisplayPass.h>
#include <KuEngine/Render/ForwardGraphTargets.h>
#include <KuEngine/Render/RenderGraph.h>
#include <KuEngine/Render/RenderGraphStatePlanner.h>
#include <KuEngine/Render/RenderPass.h>
#include <KuEngine/UI/UIOverlay.h>

#include <algorithm>
#include <cstdint>
#include <functional>
#include <string>
#include <stdexcept>
#include <utility>
#include <variant>

namespace {

class GraphTestPass final : public ku::RenderPass {
public:
    using Setup = std::function<void(ku::RenderGraphBuilder&)>;

    GraphTestPass(std::string passName, Setup setup)
        : m_name(std::move(passName)), m_setup(std::move(setup))
    {
    }

    [[nodiscard]] std::string_view name() const override { return m_name; }
    void setup(ku::RenderGraphBuilder& builder) override { m_setup(builder); }

private:
    std::string m_name;
    Setup m_setup;
};

void setupPass(ku::RenderGraph& graph, GraphTestPass& pass)
{
    const size_t passIndex = graph.registerPass(pass);
    auto builder = graph.buildPass(passIndex);
    pass.setup(builder);
}

ku::ForwardGraphTargets targetDescs()
{
    VkClearColorValue color{{0.1f, 0.2f, 0.3f, 1.0f}};
    return ku::makeForwardGraphTargets(
        VK_FORMAT_B8G8R8A8_UNORM,
        VK_FORMAT_D32_SFLOAT,
        color,
        VkClearDepthStencilValue{0.75f, 3});
}

} // namespace

TEST(ForwardGraphTargetsTest, DeclaresOwnedRelativeColorAndDepthContracts)
{
    const auto targets = targetDescs();

    EXPECT_EQ(targets.color.extent.mode, ku::ImageExtentMode::SwapchainRelative);
    EXPECT_EQ(targets.color.format, VK_FORMAT_B8G8R8A8_UNORM);
    EXPECT_EQ(
        targets.color.usage,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);
    EXPECT_EQ(targets.color.aspect, VK_IMAGE_ASPECT_COLOR_BIT);
    EXPECT_EQ(targets.color.initialContent, ku::InitialContent::Cleared);
    EXPECT_FLOAT_EQ(targets.color.clearValue.color.float32[1], 0.2f);

    EXPECT_EQ(targets.depth.extent.mode, ku::ImageExtentMode::SwapchainRelative);
    EXPECT_EQ(targets.depth.format, VK_FORMAT_D32_SFLOAT);
    EXPECT_EQ(
        targets.depth.usage,
        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);
    EXPECT_EQ(targets.depth.aspect, VK_IMAGE_ASPECT_DEPTH_BIT);
    EXPECT_EQ(targets.depth.initialContent, ku::InitialContent::Cleared);
    EXPECT_FLOAT_EQ(targets.depth.clearValue.depthStencil.depth, 0.75f);
    EXPECT_EQ(targets.depth.clearValue.depthStencil.stencil, 3u);

    EXPECT_THROW(
        (void)ku::makeForwardGraphTargets(
            VK_FORMAT_UNDEFINED,
            VK_FORMAT_D32_SFLOAT,
            {},
            {1.0f, 0}),
        std::invalid_argument);
}

TEST(ForwardGraphTargetsTest, ForwardThenDisplayProducesSampleVisibilityBarrier)
{
    const auto targets = targetDescs();
    ku::ImageDesc swapChain = targets.color;
    swapChain.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    swapChain.initialContent = ku::InitialContent::Undefined;

    ku::RenderGraph graph;
    GraphTestPass forward("Forward", [&](ku::RenderGraphBuilder& builder) {
        const auto color = builder.createImage(
            ku::forward_graph_resource::sceneColor, targets.color);
        const auto depth = builder.createImage(
            ku::forward_graph_resource::sceneDepth, targets.depth);
        builder.colorAttachment(
            color,
            ku::AttachmentLoadPolicy::Clear,
            ku::AttachmentStorePolicy::Store);
        builder.depthAttachment(
            depth,
            ku::AttachmentLoadPolicy::Clear,
            ku::AttachmentStorePolicy::DontCare);
    });
    GraphTestPass display("ForwardDisplay", [&](ku::RenderGraphBuilder& builder) {
        const auto color = builder.createImage(
            ku::forward_graph_resource::sceneColor, targets.color);
        builder.useImage(color, ku::ImageUse::FragmentSampled);
        const auto output = builder.importImage(
            ku::runtime_resource::swapChainColor, swapChain);
        builder.colorAttachment(
            output,
            ku::AttachmentLoadPolicy::Clear,
            ku::AttachmentStorePolicy::Store);
    });
    setupPass(graph, forward);
    setupPass(graph, display);
    graph.compile();

    ASSERT_TRUE(graph.findImage(ku::forward_graph_resource::sceneColor));
    const auto& sceneColor = graph.resource(
        *graph.findImage(ku::forward_graph_resource::sceneColor));
    EXPECT_EQ(sceneColor.ownership, ku::ResourceOwnership::Internal);
    ASSERT_EQ(graph.dependencies().size(), 1u);
    EXPECT_EQ(graph.dependencies()[0].fromPass, 0u);
    EXPECT_EQ(graph.dependencies()[0].toPass, 1u);

    const auto barrier = std::find_if(
        graph.barrierPlan().begin(),
        graph.barrierPlan().end(),
        [&](const ku::BarrierPlanItem& item) {
            return item.resource.index
                == graph.findImage(ku::forward_graph_resource::sceneColor)->index;
        });
    ASSERT_NE(barrier, graph.barrierPlan().end());
    EXPECT_EQ(barrier->hazard, ku::ResourceHazardType::ReadAfterWrite);
}

TEST(ForwardGraphTargetsTest, DisabledForwardFailsBeforeDisplayCanRead)
{
    const ku::ResourceHandle color{0, 1, ku::ResourceKind::Image};
    ku::CompiledResourceUse write{};
    write.resource = color;
    write.state = {
        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    write.imageRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    write.imageUse = ku::ImageUse::ColorAttachment;
    write.writes = true;
    write.attachment = true;

    ku::CompiledResourceUse read{};
    read.resource = color;
    read.state = {
        VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
        VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
    read.imageRange = write.imageRange;
    read.imageUse = ku::ImageUse::FragmentSampled;
    read.reads = true;

    EXPECT_THROW(
        (void)ku::RenderGraphStatePlanner::plan(
            {{color,
              std::string(ku::forward_graph_resource::sceneColor),
              {},
              false,
              write.imageRange,
              {}}},
            {{0, "Forward", false, {{write, false}}},
             {1, "ForwardDisplay", true, {{read, false}}}}),
        std::runtime_error);
}

TEST(ForwardGraphTargetsTest, DisplayUvMapsOnlyTheRenderedSceneRectangle)
{
    const auto full = ku::calculateForwardDisplayUvTransform(
        {0, 0, 1920, 1080}, {1920, 1080});
    EXPECT_FLOAT_EQ(full.scale[0], 1.0f);
    EXPECT_FLOAT_EQ(full.scale[1], 1.0f);
    EXPECT_FLOAT_EQ(full.offset[0], 0.0f);
    EXPECT_FLOAT_EQ(full.offset[1], 0.0f);

    const auto sidebar = ku::calculateForwardDisplayUvTransform(
        {420, 0, 1500, 1080}, {1920, 1080});
    EXPECT_FLOAT_EQ(sidebar.scale[0], 1500.0f / 1920.0f);
    EXPECT_FLOAT_EQ(sidebar.scale[1], 1.0f);
    EXPECT_FLOAT_EQ(sidebar.offset[0], 420.0f / 1920.0f);
    EXPECT_FLOAT_EQ(sidebar.offset[1], 0.0f);

    const auto highDpi = ku::calculateForwardDisplayUvTransform(
        {500, 100, 2500, 1700}, {3200, 1800});
    EXPECT_FLOAT_EQ(highDpi.scale[0], 2500.0f / 3200.0f);
    EXPECT_FLOAT_EQ(highDpi.scale[1], 1700.0f / 1800.0f);
    EXPECT_FLOAT_EQ(highDpi.offset[0], 500.0f / 3200.0f);
    EXPECT_FLOAT_EQ(highDpi.offset[1], 100.0f / 1800.0f);

    const auto tinyFallback = ku::calculateForwardDisplayUvTransform(
        {799, 599, 0, 0}, {800, 600});
    EXPECT_FLOAT_EQ(tinyFallback.scale[0], 1.0f / 800.0f);
    EXPECT_FLOAT_EQ(tinyFallback.scale[1], 1.0f / 600.0f);
    EXPECT_FLOAT_EQ(tinyFallback.offset[0], 799.0f / 800.0f);
    EXPECT_FLOAT_EQ(tinyFallback.offset[1], 599.0f / 600.0f);
}

TEST(ForwardGraphTargetsTest, DisplayRebindAndFormatPoliciesAreExplicit)
{
    const auto viewA = reinterpret_cast<VkImageView>(uintptr_t{0x100});
    const auto viewB = reinterpret_cast<VkImageView>(uintptr_t{0x200});
    EXPECT_FALSE(ku::forwardDisplaySourceChanged(7, viewA, 7, viewA));
    EXPECT_TRUE(ku::forwardDisplaySourceChanged(7, viewA, 8, viewA));
    EXPECT_TRUE(ku::forwardDisplaySourceChanged(7, viewA, 7, viewB));

    EXPECT_FALSE(ku::swapchainFormatRequiresPipelineRecompile(
        VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_B8G8R8A8_UNORM));
    EXPECT_TRUE(ku::swapchainFormatRequiresPipelineRecompile(
        VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM));
    EXPECT_EQ(ku::uiOverlayDepthFormat, VK_FORMAT_UNDEFINED);

    ku::ForwardDisplayPass display;
    const auto expected = display.expectedFrameStatistics();
    ASSERT_TRUE(expected.has_value());
    EXPECT_EQ(expected->drawCalls, 1u);
    EXPECT_EQ(expected->submittedVertices, 3u);
}
