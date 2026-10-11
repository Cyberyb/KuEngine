#include <gtest/gtest.h>
#include <KuEngine/Render/DeferredCommon.h>
#include <KuEngine/Render/DeferredGeometryPass.h>
#include <KuEngine/Render/ForwardCommon.h>
#include <KuEngine/Render/RenderGraph.h>
#include <KuEngine/Render/RenderGraphStatePlanner.h>
#include <KuEngine/Render/ForwardGraphTargets.h>
#include <KuEngine/Render/ForwardProgram.h>
#include <algorithm>
#include <stdexcept>

namespace {
class Geometry final : public ku::DeferredGeometryPass {
public:
    Geometry() { m_gbufferTargets = ku::makeGBufferTargets(VK_FORMAT_D32_SFLOAT, {0.9f, 0}); }
    void execute(ku::CommandList&, const ku::FrameData&) override {}
};
class Lighting final : public ku::RenderPass {
public:
    std::string_view name() const override { return "DeferredLighting"; }
    void setup(ku::RenderGraphBuilder& b) override {
        auto targets = ku::makeGBufferTargets(VK_FORMAT_D32_SFLOAT, {0.9f,0});
        for (size_t i = 0; i < 4; ++i)
            b.useImage(b.createImage(ku::gbufferNames[i], targets.colors[i]), ku::ImageUse::FragmentSampled);
    }
};
void setup(ku::RenderGraph& graph, ku::RenderPass& pass) {
    auto builder = graph.buildPass(graph.registerPass(pass));
    pass.setup(builder);
}
}
TEST(DeferredTest, PublicLayoutAndEmptyEncodingAreExact) {
    const auto targets = ku::makeGBufferTargets(VK_FORMAT_D32_SFLOAT, {0.9f,0});
    ASSERT_EQ(targets.colors.size(), 4u);
    for (const auto& color : targets.colors) {
        EXPECT_EQ(color.format, VK_FORMAT_R16G16B16A16_SFLOAT);
        EXPECT_EQ(color.extent.mode, ku::ImageExtentMode::SwapchainRelative);
        EXPECT_EQ(color.usage, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
        EXPECT_EQ(color.aspect, VK_IMAGE_ASPECT_COLOR_BIT);
        EXPECT_EQ(color.initialContent, ku::InitialContent::Cleared);
        EXPECT_FLOAT_EQ(color.clearValue.color.float32[3], 0.0f);
    }
    EXPECT_EQ(ku::encodeGBufferShadingModel(ku::asset::ShadingModel::PBR), ku::GBufferShadingCode::PBR);
    EXPECT_EQ(ku::encodeGBufferShadingModel(ku::asset::ShadingModel::Unlit), ku::GBufferShadingCode::Unlit);
    EXPECT_FALSE(ku::encodeGBufferShadingModel(static_cast<ku::asset::ShadingModel>(99)));
    EXPECT_EQ(static_cast<uint32_t>(ku::GBufferShadingCode::Empty), 0u);
    EXPECT_EQ(static_cast<uint32_t>(ku::GBufferShadingCode::PBR), 1u);
    EXPECT_EQ(static_cast<uint32_t>(ku::GBufferShadingCode::Unlit), 2u);
    EXPECT_EQ(targets.depth.format, VK_FORMAT_D32_SFLOAT);
    EXPECT_FLOAT_EQ(targets.depth.clearValue.depthStencil.depth, 0.9f);
    EXPECT_THROW((void)ku::makeGBufferTargets(VK_FORMAT_UNDEFINED, {}), std::invalid_argument);
}
TEST(DeferredTest, OpaqueMaskAndModelsSupportedButBlendRejected) {
    for (auto model : {ku::asset::ShadingModel::PBR, ku::asset::ShadingModel::Unlit}) {
        for (bool doubleSided : {false, true}) {
            EXPECT_TRUE(ku::supportsDeferredGeometry({model, ku::asset::AlphaMode::Opaque, doubleSided}));
            EXPECT_TRUE(ku::supportsDeferredGeometry({model, ku::asset::AlphaMode::Mask, doubleSided}));
            EXPECT_FALSE(ku::supportsDeferredGeometry({model, ku::asset::AlphaMode::Blend, doubleSided}));
        }
    }
    EXPECT_FALSE(ku::supportsDeferredGeometry({static_cast<ku::asset::ShadingModel>(99),ku::asset::AlphaMode::Opaque,false}));
    ku::ForwardProgram geometry;
    // The CPU predicate is the same gate used by ensurePipelines and render.
    EXPECT_FALSE(geometry.geometryMode());
}
TEST(DeferredTest, DeviceLimitsAndFormatArePreflighted) {
    VkPhysicalDeviceLimits limits{};
    limits.maxColorAttachments = limits.maxFragmentOutputAttachments = 4;
    constexpr VkFormatFeatureFlags good = VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT | VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT | VK_FORMAT_FEATURE_TRANSFER_SRC_BIT;
    EXPECT_NO_THROW(ku::validateGBufferCapabilities(limits, good));
    EXPECT_THROW(ku::validateGBufferCapabilities(limits, good & ~VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT), std::runtime_error);
    EXPECT_THROW(ku::validateGBufferCapabilities(limits, good & ~VK_FORMAT_FEATURE_TRANSFER_SRC_BIT), std::runtime_error);
    limits.maxColorAttachments = 3;
    EXPECT_THROW(ku::validateGBufferCapabilities(limits, good), std::runtime_error);
    limits.maxColorAttachments = 4;
    limits.maxFragmentOutputAttachments = 3;
    EXPECT_THROW(ku::validateGBufferCapabilities(limits, good), std::runtime_error);
}
TEST(DeferredTest, GeometryMrtDepthLightingDependenciesAndDisabledProducer) {
    ku::RenderGraph graph;
    Geometry geometry;
    Lighting lighting;
    setup(graph, geometry); setup(graph, lighting); graph.compile();
    ASSERT_EQ(graph.passes()[0].attachments.size(), 5u);
    for (size_t i = 0; i < 4; ++i) {
        const auto h = graph.findImage(ku::gbufferNames[i]);
        ASSERT_TRUE(h);
        EXPECT_EQ(graph.passes()[0].attachments[i].resource, *h);
        EXPECT_EQ(graph.passes()[0].attachments[i].type, ku::AttachmentType::Color);
        EXPECT_EQ(graph.passes()[0].attachments[i].storePolicy, ku::AttachmentStorePolicy::Store);
    }
    EXPECT_EQ(graph.passes()[0].attachments[4].type, ku::AttachmentType::Depth);
    ASSERT_EQ(graph.dependencies().size(), 1u);
    EXPECT_EQ(graph.dependencies()[0].fromPass, 0u);
    EXPECT_EQ(graph.dependencies()[0].toPass, 1u);
    EXPECT_EQ(graph.barrierPlan().size(), 4u);
    std::vector<ku::StatePlannerResource> resources;
    for (uint32_t i = 0; i < graph.resources().size(); ++i) {
        const auto& r = graph.resources()[i];
        resources.push_back({{i,graph.generation(),ku::ResourceKind::Image},r.name,{},false,
            ku::RenderGraph::normalizeImageRange(std::get<ku::ImageDesc>(r.description),{}),{}});
    }
    std::vector<ku::StatePlannerPass> passes;
    for (size_t i = 0; i < graph.passes().size(); ++i) {
        ku::StatePlannerPass pass{i,graph.passes()[i].name,true,{}};
        for (const auto& use : graph.passes()[i].uses) pass.uses.push_back({use,false});
        passes.push_back(pass);
    }
    const auto plan = ku::RenderGraphStatePlanner::plan(resources, passes);
    ASSERT_EQ(plan.barriersBeforePass[1].size(), 4u);
    for (const auto& barrier : plan.barriersBeforePass[1]) {
        EXPECT_EQ(barrier.after.layout, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        EXPECT_EQ(barrier.after.access, VK_ACCESS_2_SHADER_SAMPLED_READ_BIT);
    }
    passes[0].enabled = false;
    EXPECT_THROW((void)ku::RenderGraphStatePlanner::plan(resources, passes), std::runtime_error);
}
TEST(DeferredTest, SharedFrameUniformPackingClampsPointLightsAndPreservesOutput) {
    ku::ForwardView view;
    view.cameraPosition = {1,2,3};
    view.environmentEnabled = true;
    view.environmentExposure = 2.0f;
    view.lighting.pointLights.resize(6);
    const auto frame = ku::makeForwardFrameUniforms(view);
    EXPECT_FLOAT_EQ(frame.cameraPosition[2],3.0f);
    EXPECT_FLOAT_EQ(frame.directionalColorPointCount[3],4.0f);
    EXPECT_FLOAT_EQ(frame.environmentParams[0],1.0f);
    EXPECT_FLOAT_EQ(frame.environmentParams[2],2.0f);
}
