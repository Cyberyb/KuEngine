#include <gtest/gtest.h>

#include <KuEngine/Render/RenderGraph.h>
#include <KuEngine/Render/RenderPass.h>

#include <functional>
#include <stdexcept>
#include <string>
#include <utility>

namespace {

class TestPass final : public ku::RenderPass {
public:
    using SetupFn = std::function<void(ku::RenderGraphBuilder&)>;

    explicit TestPass(std::string name, SetupFn setup = {})
        : m_name(std::move(name))
        , m_setup(std::move(setup))
    {
    }

    [[nodiscard]] std::string_view name() const override
    {
        return m_name;
    }

    void setup(ku::RenderGraphBuilder& builder) override
    {
        if (m_setup) {
            m_setup(builder);
        }
    }

private:
    std::string m_name;
    SetupFn m_setup;
};

} // namespace

TEST(RenderGraphTest, ExplicitDependencyAffectsExecutionOrder)
{
    ku::RenderGraph graph;

    TestPass passB("PassB");
    TestPass passA("PassA", [](ku::RenderGraphBuilder& builder) {
        builder.dependsOn("PassB");
    });

    const size_t passAIndex = graph.registerPass(passA);
    const size_t passBIndex = graph.registerPass(passB);

    auto setupA = graph.buildPass(passAIndex);
    passA.setup(setupA);

    auto setupB = graph.buildPass(passBIndex);
    passB.setup(setupB);

    graph.compile();

    ASSERT_EQ(graph.executionOrder().size(), 2u);
    EXPECT_EQ(graph.executionOrder()[0], passBIndex);
    EXPECT_EQ(graph.executionOrder()[1], passAIndex);
}

TEST(RenderGraphTest, BuildsDependenciesAndBarrierPlanFromResourceAccess)
{
    ku::RenderGraph graph;

    TestPass geometry("Geometry", [](ku::RenderGraphBuilder& builder) {
        const ku::ResourceHandle color = builder.createResource("Color");
        builder.write(color);
    });

    TestPass lighting("Lighting", [](ku::RenderGraphBuilder& builder) {
        const ku::ResourceHandle color = builder.createResource("Color");
        const ku::ResourceHandle lit = builder.createResource("ColorLit");
        builder.read(color);
        builder.write(lit);
    });

    TestPass toneMap("ToneMap", [](ku::RenderGraphBuilder& builder) {
        const ku::ResourceHandle lit = builder.createResource("ColorLit");
        const ku::ResourceHandle swap = builder.importExternal("SwapChainColor");
        builder.read(lit);
        builder.write(swap);
    });

    const size_t geometryIndex = graph.registerPass(geometry);
    const size_t lightingIndex = graph.registerPass(lighting);
    const size_t toneMapIndex = graph.registerPass(toneMap);

    auto setupGeometry = graph.buildPass(geometryIndex);
    geometry.setup(setupGeometry);
    auto setupLighting = graph.buildPass(lightingIndex);
    lighting.setup(setupLighting);
    auto setupToneMap = graph.buildPass(toneMapIndex);
    toneMap.setup(setupToneMap);

    graph.compile();

    ASSERT_EQ(graph.executionOrder().size(), 3u);
    EXPECT_EQ(graph.executionOrder()[0], geometryIndex);
    EXPECT_EQ(graph.executionOrder()[1], lightingIndex);
    EXPECT_EQ(graph.executionOrder()[2], toneMapIndex);

    EXPECT_EQ(graph.dependencies().size(), 2u);
    EXPECT_EQ(graph.barrierPlan().size(), 2u);
}

TEST(RenderGraphTest, ThrowsWhenExplicitDependencyTargetMissing)
{
    ku::RenderGraph graph;

    TestPass passA("PassA", [](ku::RenderGraphBuilder& builder) {
        builder.dependsOn("MissingPass");
    });

    const size_t passAIndex = graph.registerPass(passA);
    auto setupA = graph.buildPass(passAIndex);
    passA.setup(setupA);

    EXPECT_THROW(graph.compile(), std::runtime_error);
}

TEST(RenderGraphTest, DetectsCycleFromExplicitDependencies)
{
    ku::RenderGraph graph;

    TestPass passA("PassA", [](ku::RenderGraphBuilder& builder) {
        builder.dependsOn("PassB");
    });

    TestPass passB("PassB", [](ku::RenderGraphBuilder& builder) {
        builder.dependsOn("PassA");
    });

    const size_t passAIndex = graph.registerPass(passA);
    const size_t passBIndex = graph.registerPass(passB);

    auto setupA = graph.buildPass(passAIndex);
    passA.setup(setupA);
    auto setupB = graph.buildPass(passBIndex);
    passB.setup(setupB);

    EXPECT_THROW(graph.compile(), std::runtime_error);
}

TEST(RenderGraphTest, GeneratesExpectedHazardTypesForSharedResource)
{
    ku::RenderGraph graph;

    TestPass passA("PassA", [](ku::RenderGraphBuilder& builder) {
        const ku::ResourceHandle color = builder.createResource("Color");
        builder.write(color);
    });

    TestPass passB("PassB", [](ku::RenderGraphBuilder& builder) {
        const ku::ResourceHandle color = builder.createResource("Color");
        builder.read(color);
    });

    TestPass passC("PassC", [](ku::RenderGraphBuilder& builder) {
        const ku::ResourceHandle color = builder.createResource("Color");
        builder.write(color);
    });

    const size_t passAIndex = graph.registerPass(passA);
    const size_t passBIndex = graph.registerPass(passB);
    const size_t passCIndex = graph.registerPass(passC);

    auto setupA = graph.buildPass(passAIndex);
    passA.setup(setupA);
    auto setupB = graph.buildPass(passBIndex);
    passB.setup(setupB);
    auto setupC = graph.buildPass(passCIndex);
    passC.setup(setupC);

    graph.compile();

    ASSERT_EQ(graph.dependencies().size(), 3u);
    ASSERT_EQ(graph.barrierPlan().size(), 3u);

    auto findHazard = [&](size_t fromPass, size_t toPass) {
        for (const ku::BarrierPlanItem& item : graph.barrierPlan()) {
            if (item.fromPass == fromPass && item.toPass == toPass) {
                return item.hazard;
            }
        }

        throw std::runtime_error("hazard item not found");
    };

    EXPECT_EQ(findHazard(passAIndex, passBIndex), ku::ResourceHazardType::ReadAfterWrite);
    EXPECT_EQ(findHazard(passAIndex, passCIndex), ku::ResourceHazardType::WriteAfterWrite);
    EXPECT_EQ(findHazard(passBIndex, passCIndex), ku::ResourceHazardType::WriteAfterRead);
}

TEST(RenderGraphTest, MarksImportedResourceAsExternal)
{
    ku::RenderGraph graph;

    TestPass writer("Writer", [](ku::RenderGraphBuilder& builder) {
        const ku::ResourceHandle swap = builder.importExternal("SwapChainColor");
        builder.write(swap);
    });

    TestPass reader("Reader", [](ku::RenderGraphBuilder& builder) {
        const ku::ResourceHandle swap = builder.createResource("SwapChainColor");
        builder.read(swap);
    });

    const size_t writerIndex = graph.registerPass(writer);
    const size_t readerIndex = graph.registerPass(reader);

    auto setupWriter = graph.buildPass(writerIndex);
    writer.setup(setupWriter);
    auto setupReader = graph.buildPass(readerIndex);
    reader.setup(setupReader);

    graph.compile();

    ASSERT_EQ(graph.resources().size(), 1u);
    EXPECT_TRUE(graph.resources()[0].external);
    ASSERT_EQ(graph.dependencies().size(), 1u);
    EXPECT_EQ(graph.dependencies()[0].fromPass, writerIndex);
    EXPECT_EQ(graph.dependencies()[0].toPass, readerIndex);
}
