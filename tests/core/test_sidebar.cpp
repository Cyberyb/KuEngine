#include <gtest/gtest.h>

#include <KuEngine/Render/RenderPipeline.h>
#include <KuEngine/UI/SidebarState.h>

#include <imgui.h>

#include <array>
#include <string_view>

namespace {

class CountingUIPass final : public ku::RenderPass {
public:
    explicit CountingUIPass(std::string_view passName)
        : m_passName(passName)
    {
    }

    [[nodiscard]] std::string_view name() const override
    {
        return m_passName;
    }

    void drawUI() override
    {
        ++drawUICount;
        ImGui::TextUnformatted("Counting pass content");
    }

    int drawUICount = 0;

private:
    std::string_view m_passName;
};

class ImGuiContextScope {
public:
    ImGuiContextScope()
    {
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.IniFilename = nullptr;
        io.DisplaySize = ImVec2(1280.0f, 720.0f);
        io.DeltaTime = 1.0f / 60.0f;
        unsigned char* pixels = nullptr;
        int width = 0;
        int height = 0;
        io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    }

    ~ImGuiContextScope()
    {
        ImGui::DestroyContext();
    }

    ImGuiContextScope(const ImGuiContextScope&) = delete;
    ImGuiContextScope& operator=(const ImGuiContextScope&) = delete;
};

} // namespace

TEST(SidebarStateTest, ExpandedCollapseAndReopenRoundTrip)
{
    ku::SidebarState state;
    EXPECT_TRUE(state.expanded());
    EXPECT_EQ(state.layoutMode(true), ku::SidebarLayoutMode::Expanded);

    state.collapse();
    EXPECT_FALSE(state.expanded());
    EXPECT_EQ(state.layoutMode(true), ku::SidebarLayoutMode::Compact);
    EXPECT_TRUE(state.sections(true).reopenControl);

    state.expand();
    EXPECT_TRUE(state.expanded());
    EXPECT_FALSE(state.sections(true).reopenControl);
}

TEST(SidebarStateTest, CompactStatsCanBeHiddenWithoutLosingReopenControl)
{
    ku::SidebarState state;
    state.collapse();
    state.setCompactStatsVisible(false);

    const ku::SidebarSections sections = state.sections(true);
    EXPECT_FALSE(sections.performance);
    EXPECT_TRUE(sections.reopenControl);
    EXPECT_EQ(state.layoutMode(true), ku::SidebarLayoutMode::ReopenOnly);

    state.setCompactStatsVisible(true);
    EXPECT_TRUE(state.sections(true).performance);
    EXPECT_EQ(state.layoutMode(true), ku::SidebarLayoutMode::Compact);
}

TEST(SidebarStateTest, StatisticsGateDoesNotHideControlsOrGraph)
{
    ku::SidebarState state;
    const ku::SidebarSections sections = state.sections(false);

    EXPECT_FALSE(sections.performance);
    EXPECT_TRUE(sections.parameters);
    EXPECT_TRUE(sections.renderGraph);
}

TEST(SidebarLayoutPolicyTest, ClampsWidthAndMaintainsRightAnchor)
{
    const ku::SidebarLayoutPolicy policy;

    const ku::SidebarLayout normal = policy.calculate(
        1280.0f,
        720.0f,
        ku::SidebarLayoutMode::Expanded);
    EXPECT_FLOAT_EQ(normal.width, 384.0f);
    EXPECT_FLOAT_EQ(normal.x + normal.width, 1280.0f);
    EXPECT_FLOAT_EQ(normal.height, 720.0f);

    const ku::SidebarLayout wide = policy.calculate(
        4000.0f,
        1000.0f,
        ku::SidebarLayoutMode::Expanded);
    EXPECT_FLOAT_EQ(wide.width, 440.0f);
    EXPECT_FLOAT_EQ(wide.x + wide.width, 4000.0f);

    const ku::SidebarLayout small = policy.calculate(
        200.0f,
        100.0f,
        ku::SidebarLayoutMode::Expanded);
    EXPECT_FLOAT_EQ(small.x, 0.0f);
    EXPECT_FLOAT_EQ(small.width, 200.0f);
    EXPECT_FLOAT_EQ(small.height, 100.0f);
}

TEST(SidebarLayoutPolicyTest, CompactAndReopenLayoutsFitSmallWindows)
{
    const ku::SidebarLayoutPolicy policy;

    const ku::SidebarLayout compact = policy.calculate(
        240.0f,
        120.0f,
        ku::SidebarLayoutMode::Compact);
    EXPECT_FLOAT_EQ(compact.x, 0.0f);
    EXPECT_FLOAT_EQ(compact.width, 240.0f);
    EXPECT_FLOAT_EQ(compact.height, 120.0f);

    const ku::SidebarLayout reopen = policy.calculate(
        160.0f,
        40.0f,
        ku::SidebarLayoutMode::ReopenOnly);
    EXPECT_FLOAT_EQ(reopen.x, 0.0f);
    EXPECT_FLOAT_EQ(reopen.width, 160.0f);
    EXPECT_FLOAT_EQ(reopen.height, 40.0f);
}

TEST(SidebarFrameLayoutTest, StateChangesApplyOnlyToNextSnapshot)
{
    ku::SidebarState state;
    const ku::SidebarLayoutPolicy policy;
    const ku::SidebarFrameLayout current = ku::describeSidebarFrame(
        state,
        policy,
        1280.0f,
        720.0f,
        true);

    state.collapse();
    const ku::SidebarFrameLayout next = ku::describeSidebarFrame(
        state,
        policy,
        1280.0f,
        720.0f,
        true);

    EXPECT_EQ(current.mode, ku::SidebarLayoutMode::Expanded);
    EXPECT_GT(current.reservedSceneLogicalWidth, 0.0f);
    EXPECT_EQ(next.mode, ku::SidebarLayoutMode::Compact);
    EXPECT_FLOAT_EQ(next.reservedSceneLogicalWidth, 0.0f);
}

TEST(RenderPipelineUITest, DrawsEachEnabledPassContentOncePerFrame)
{
    ImGuiContextScope imgui;
    ku::RenderPipeline pipeline;

    // The first label intentionally points at a non-null-terminated view.
    const std::array<char, 8> labelStorage{
        'S', 'h', 'a', 'r', 'e', 'd', 'X', 'Y'};
    auto& enabled = pipeline.addPass<CountingUIPass>(
        std::string_view(labelStorage.data(), 6));
    auto& disabled = pipeline.addPass<CountingUIPass>("Shared");
    disabled.setEnabled(false);

    ImGui::NewFrame();
    ImGui::Begin("Sidebar test host");
    pipeline.drawPassUIContent();
    ImGui::End();
    ImGui::Render();

    EXPECT_EQ(enabled.drawUICount, 1);
    EXPECT_EQ(disabled.drawUICount, 0);

    ImGui::NewFrame();
    ImGui::Begin("Sidebar test host");
    pipeline.drawPassUIContent();
    ImGui::End();
    ImGui::Render();

    EXPECT_EQ(enabled.drawUICount, 2);
    EXPECT_EQ(disabled.drawUICount, 0);
}
