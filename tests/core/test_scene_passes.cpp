#include <gtest/gtest.h>
#include <KuEngine/Render/ScenePasses.h>
#include <KuEngine/Render/ForwardGraphTargets.h>
#include <KuEngine/Render/RenderPass.h>
#include <stdexcept>
namespace {
class Source : public ku::SceneDrawSource {
public:
    ku::SceneRenderSnapshot snapshot{};
    bool active=true;
    std::string_view scenePrepareName() const override {return "Prepare";}
    const ku::SceneRenderSnapshot& sceneSnapshot() const override {return snapshot;}
    bool sceneSourceEnabled() const override {return active;}
    ku::ForwardRenderer& sceneRenderer(ku::SceneDrawPhase) override {throw std::runtime_error("CPU test must not draw");}
};
}
TEST(ScenePasses, DisabledUnpreparedAndStaleSnapshotsFailClearly) {
    Source source; ku::FrameData frame{}; frame.totalTime=1;
    EXPECT_THROW((void)source.checkedSceneSnapshot(frame),std::runtime_error);
    source.snapshot.preparedTime=1;
    EXPECT_NO_THROW((void)source.checkedSceneSnapshot(frame));
    source.active=false;
    EXPECT_THROW((void)source.checkedSceneSnapshot(frame),std::runtime_error);
    source.active=true; frame.totalTime=2;
    EXPECT_THROW((void)source.checkedSceneSnapshot(frame),std::runtime_error);
}
TEST(ScenePasses, HdrBlendCapabilityAndEncodingAreExplicit) {
    constexpr auto features=VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT|VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT|VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT|VK_FORMAT_FEATURE_TRANSFER_SRC_BIT;
    EXPECT_NO_THROW(ku::validateSceneHdrCapabilities(features));
    for(auto flag:{VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT,VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT,VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT,VK_FORMAT_FEATURE_TRANSFER_SRC_BIT})
        EXPECT_THROW(ku::validateSceneHdrCapabilities(features & ~flag),std::runtime_error);
    EXPECT_TRUE(ku::needsManualDisplayEncoding(VK_FORMAT_R8G8B8A8_UNORM,true));
    EXPECT_FALSE(ku::needsManualDisplayEncoding(VK_FORMAT_R8G8B8A8_SRGB,true));
    EXPECT_FALSE(ku::needsManualDisplayEncoding(VK_FORMAT_B8G8R8A8_SRGB,false));
    EXPECT_FALSE(ku::needsManualDisplayEncoding(VK_FORMAT_B8G8R8A8_UNORM,false));
    EXPECT_EQ(ku::sceneHdrFormat,VK_FORMAT_R16G16B16A16_SFLOAT);
}
TEST(ScenePasses, ConsumersExplicitlyDependOnPrepareWithoutFakeGpuResources) {
    Source source;
    ku::RenderGraph graph; class Prepare : public ku::RenderPass { public: std::string_view name()const override{return "Prepare";} } prepare;
    auto p=graph.registerPass(prepare); auto pb=graph.buildPass(p);prepare.setup(pb);
    Prepare consumer;
    auto b=graph.registerPass(consumer);auto bb=graph.buildPass(b);source.declarePrepareDependency(bb);
    EXPECT_TRUE(graph.passes()[0].uses.empty());
    ASSERT_EQ(graph.passes()[1].explicitDependencies.size(),1u);
    EXPECT_EQ(graph.passes()[1].explicitDependencies.front(),"Prepare");
    EXPECT_THROW((void)ku::SceneSurfacePass(source,ku::SceneDrawPhase::Background),std::invalid_argument);
    EXPECT_THROW((void)ku::SceneSurfacePass(source,ku::SceneDrawPhase::Blend,true),std::invalid_argument);
}
