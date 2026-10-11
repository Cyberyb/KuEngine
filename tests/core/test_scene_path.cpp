#include <gtest/gtest.h>
#include <KuEngine/Asset/AssetConfig.h>
#include <KuEngine/Render/ScenePathControl.h>
#include <KuEngine/Render/RenderPass.h>
#include <filesystem>
#include <array>
namespace {
class Pass : public ku::RenderPass { public: std::string_view name()const override{return "CPU";} };
using Path=ku::asset::RenderPath;
using View=ku::GBufferView;
}
TEST(ScenePath, ConfigDefaultLegalAndStrictFailures) {
    ku::asset::SceneConfig config; std::string error;
    const auto root=std::filesystem::path(KUENGINE_SOURCE_DIR)/"tests/fixtures";
    EXPECT_EQ(config.renderPath,Path::Forward);
    ASSERT_TRUE(ku::asset::loadSceneConfigFromFile(root/"render-path-default.json",config,&error)); EXPECT_EQ(config.renderPath,Path::Forward);
    ASSERT_TRUE(ku::asset::loadSceneConfigFromFile(root/"render-path-deferred.json",config,&error)); EXPECT_EQ(config.renderPath,Path::Deferred);
    ASSERT_TRUE(ku::asset::loadSceneConfigFromFile(root/"render-path-forward.json",config,&error)); EXPECT_EQ(config.renderPath,Path::Forward);
    for(const char* file:{"render-path-invalid.json","render-path-type.json"}) { EXPECT_FALSE(ku::asset::loadSceneConfigFromFile(root/file,config,&error)); EXPECT_FALSE(error.empty()); }
}
TEST(ScenePath, CliPrecedenceAliasAndConflicts) {
    using namespace std::literals;
    const std::array args{"--render-path"sv,"forward"sv,"--smoke-frames"sv,"3"sv};
    auto options=ku::asset::parseRenderPathCommandLine(args);
    EXPECT_EQ(ku::asset::resolveRenderPath(Path::Deferred,options.overridePath),Path::Forward);
    EXPECT_EQ(options.forwarded,(std::vector<std::string>{"--smoke-frames","3"}));
    EXPECT_EQ(ku::asset::resolveRenderPath(Path::Deferred,std::nullopt),Path::Deferred);
    const std::array alias{"--deferred"sv,"--render-path"sv,"deferred"sv};
    EXPECT_EQ(ku::asset::parseRenderPathCommandLine(alias).overridePath,Path::Deferred);
    const std::array conflict{"--deferred"sv,"--render-path"sv,"forward"sv};
    EXPECT_THROW((void)ku::asset::parseRenderPathCommandLine(conflict),std::invalid_argument);
    const std::array invalid{"--render-path"sv,"DEFERRED"sv};
    EXPECT_THROW((void)ku::asset::parseRenderPathCommandLine(invalid),std::invalid_argument);
    const std::array missing{"--render-path"sv};
    EXPECT_THROW((void)ku::asset::parseRenderPathCommandLine(missing),std::invalid_argument);
    const std::array conflictingExplicit{"--render-path"sv,"forward"sv,"--render-path"sv,"deferred"sv};
    EXPECT_THROW((void)ku::asset::parseRenderPathCommandLine(conflictingExplicit),std::invalid_argument);
    const std::array repeated{"--render-path"sv,"forward"sv,"--render-path"sv,"forward"sv};
    EXPECT_EQ(ku::asset::parseRenderPathCommandLine(repeated).overridePath,Path::Forward);
}
TEST(ScenePath, DefaultConfigReloadAndMaskValidationAreStable) {
    ku::asset::SceneConfig config;config.renderPath=Path::Deferred;std::string error;
    ASSERT_TRUE(ku::asset::loadSceneConfigFromFile(std::filesystem::path(KUENGINE_SOURCE_DIR)/"tests/fixtures/render-path-default.json",config,&error));
    EXPECT_EQ(config.renderPath,Path::Forward);
    EXPECT_EQ(ku::sceneBranchMask(Path::Forward,View::Scene),(ku::SceneBranchMask{true,false,false,true,false}));
    EXPECT_EQ(ku::sceneBranchMask(Path::Deferred,View::Scene),(ku::SceneBranchMask{false,true,true,true,false}));
    EXPECT_THROW((void)ku::sceneBranchMask(Path::Forward,View::Normal),std::invalid_argument);
    EXPECT_THROW((void)ku::sceneBranchMask(Path::Deferred,static_cast<View>(9)),std::invalid_argument);
    EXPECT_STREQ(ku::gbufferViewLabel(static_cast<View>(99)),"Invalid");
}
TEST(ScenePath, PendingUiRequestPublishesOnceAndUsesExclusiveMask) {
    Pass f,g,l,d,debug; ku::ScenePathControl control; control.bind(f,&g,&l,d,&debug); control.initialize(Path::Forward);
    ASSERT_TRUE(control.request(Path::Deferred,View::Normal));
    EXPECT_EQ(control.path(),Path::Forward); EXPECT_TRUE(f.enabled()); EXPECT_FALSE(g.enabled());
    auto generation=control.generation(); EXPECT_TRUE(control.publishPending()); EXPECT_FALSE(control.publishPending()); EXPECT_EQ(control.generation(),generation+1);
    EXPECT_FALSE(f.enabled()); EXPECT_TRUE(g.enabled()); EXPECT_TRUE(l.enabled()); EXPECT_FALSE(d.enabled()); EXPECT_TRUE(debug.enabled());
    for(uint32_t i=1;i<=8;++i) { EXPECT_TRUE(control.request(Path::Deferred,static_cast<View>(i))); EXPECT_TRUE(control.publishPending()); EXPECT_EQ(control.view(),static_cast<View>(i)); }
    EXPECT_TRUE(control.request(Path::Forward)); control.publishPending(); EXPECT_EQ(control.view(),View::Scene); EXPECT_TRUE(d.enabled()); EXPECT_FALSE(debug.enabled());
}
TEST(ScenePath, RejectedRequestsPreserveCurrentPendingMaskAndDebug) {
    Pass f,g,l,d,debug; ku::ScenePathControl control; control.bind(f,&g,&l,d,&debug); control.initialize(Path::Deferred);
    ASSERT_TRUE(control.request(Path::Deferred,View::Ao)); control.publishPending(); const auto generation=control.generation();
    EXPECT_FALSE(control.request(Path::Forward,View::Base)); EXPECT_FALSE(control.request(Path::Deferred,static_cast<View>(9)));
    EXPECT_FALSE(control.request(static_cast<Path>(9))); EXPECT_FALSE(control.publishPending());
    EXPECT_EQ(control.generation(),generation); EXPECT_EQ(control.view(),View::Ao); EXPECT_TRUE(debug.enabled()); EXPECT_FALSE(f.enabled());
    ASSERT_TRUE(control.request(Path::Forward)); EXPECT_FALSE(control.request(Path::Forward,View::Base)); EXPECT_TRUE(control.publishPending()); EXPECT_EQ(control.path(),Path::Forward);
    control.setDeferredCapability(false,"four attachments unavailable");
    EXPECT_FALSE(control.request(Path::Deferred)); EXPECT_FALSE(control.publishPending()); EXPECT_EQ(control.path(),Path::Forward); EXPECT_TRUE(f.enabled());
    EXPECT_THROW(control.initialize(Path::Deferred),std::runtime_error);
    ku::ScenePathControl incomplete; incomplete.bind(f,nullptr,nullptr,d,nullptr);
    EXPECT_FALSE(incomplete.request(Path::Deferred)); EXPECT_TRUE(incomplete.request(Path::Forward)); EXPECT_TRUE(incomplete.publishPending());
}
