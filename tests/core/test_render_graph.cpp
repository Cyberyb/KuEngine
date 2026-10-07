#include <gtest/gtest.h>

#include <KuEngine/Render/RenderGraph.h>
#include <KuEngine/Render/RenderGraphResourcePool.h>
#include <KuEngine/Render/RenderGraphResources.h>
#include <KuEngine/Render/RenderGraphStatePlanner.h>
#include <KuEngine/Render/RenderPass.h>
#include <KuEngine/Render/RenderPipeline.h>
#include <KuEngine/RHI/RHIDevice.h>

#include <algorithm>
#include <cstdint>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

namespace {

ku::ImageDesc colorDesc(
    ku::ImageExtentDesc extent = ku::ImageExtentDesc::absoluteExtent(64, 64),
    ku::InitialContent initial = ku::InitialContent::Undefined)
{
    ku::ImageDesc desc{};
    desc.extent = extent;
    desc.format = VK_FORMAT_R8G8B8A8_UNORM;
    desc.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
        | VK_IMAGE_USAGE_SAMPLED_BIT
        | VK_IMAGE_USAGE_TRANSFER_SRC_BIT
        | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    desc.aspect = VK_IMAGE_ASPECT_COLOR_BIT;
    desc.initialContent = initial;
    return desc;
}

ku::ImageDesc depthDesc()
{
    ku::ImageDesc desc{};
    desc.extent = ku::ImageExtentDesc::absoluteExtent(64, 64);
    desc.format = VK_FORMAT_D32_SFLOAT;
    desc.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    desc.aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
    return desc;
}

ku::BufferDesc bufferDesc(
    ku::InitialContent initial = ku::InitialContent::Undefined)
{
    return ku::BufferDesc{
        .size = 256,
        .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
            | VK_BUFFER_USAGE_TRANSFER_SRC_BIT
            | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        .memoryUsage = VMA_MEMORY_USAGE_AUTO,
        .initialContent = initial,
    };
}

class TestPass final : public ku::RenderPass {
public:
    using SetupFn = std::function<void(ku::RenderGraphBuilder&)>;

    explicit TestPass(std::string name, SetupFn setup = {})
        : m_name(std::move(name)), m_setup(std::move(setup)) {}

    [[nodiscard]] std::string_view name() const override { return m_name; }
    void setup(ku::RenderGraphBuilder& builder) override
    {
        if (m_setup) m_setup(builder);
    }

private:
    std::string m_name;
    SetupFn m_setup;
};

void setupPass(ku::RenderGraph& graph, TestPass& pass)
{
    const size_t index = graph.registerPass(pass);
    auto builder = graph.buildPass(index);
    pass.setup(builder);
}

struct AllocationCounts {
    size_t imagesCreated = 0;
    size_t imagesDestroyed = 0;
    size_t buffersCreated = 0;
    size_t buffersDestroyed = 0;
};

class FakeImageAllocation final : public ku::RenderGraphImageAllocation {
public:
    FakeImageAllocation(AllocationCounts& counts, uintptr_t identity)
        : m_counts(&counts), m_identity(identity) {}
    ~FakeImageAllocation() override { ++m_counts->imagesDestroyed; }
    [[nodiscard]] VkImage image() const noexcept override
    {
        return reinterpret_cast<VkImage>(m_identity * 2 + 1);
    }
    [[nodiscard]] VkImageView imageView() const noexcept override
    {
        return reinterpret_cast<VkImageView>(m_identity * 2 + 2);
    }
private:
    AllocationCounts* m_counts;
    uintptr_t m_identity;
};

class FakeBufferAllocation final : public ku::RenderGraphBufferAllocation {
public:
    FakeBufferAllocation(AllocationCounts& counts, uintptr_t identity)
        : m_counts(&counts), m_identity(identity) {}
    ~FakeBufferAllocation() override { ++m_counts->buffersDestroyed; }
    [[nodiscard]] VkBuffer buffer() const noexcept override
    {
        return reinterpret_cast<VkBuffer>(m_identity);
    }
private:
    AllocationCounts* m_counts;
    uintptr_t m_identity;
};

class FakeAllocator final : public ku::RenderGraphResourceAllocator {
public:
    explicit FakeAllocator(AllocationCounts& counts) : m_counts(&counts) {}

    size_t failOnCall = 0;

    std::unique_ptr<ku::RenderGraphImageAllocation> createImage(
        const ku::ImageDesc&,
        VkExtent2D) override
    {
        failIfRequested();
        ++m_counts->imagesCreated;
        return std::make_unique<FakeImageAllocation>(*m_counts, ++m_identity);
    }

    std::unique_ptr<ku::RenderGraphBufferAllocation> createBuffer(
        const ku::BufferDesc&) override
    {
        failIfRequested();
        ++m_counts->buffersCreated;
        return std::make_unique<FakeBufferAllocation>(*m_counts, ++m_identity);
    }

private:
    void failIfRequested()
    {
        ++m_call;
        if (failOnCall != 0 && m_call == failOnCall) {
            throw std::runtime_error("injected allocation failure");
        }
    }

    AllocationCounts* m_counts;
    size_t m_call = 0;
    uintptr_t m_identity = 100;
};

struct PoolFixtureGraph {
    ku::RenderGraph graph;
    ku::ImageHandle absoluteImage{};
    ku::ImageHandle relativeImage{};
    ku::ImageHandle relativeImage2{};
    ku::BufferHandle buffer{};

    PoolFixtureGraph()
    {
        TestPass pass("Allocate", [this](ku::RenderGraphBuilder& builder) {
            absoluteImage = builder.createImage("Absolute", colorDesc());
            relativeImage = builder.createImage(
                "Relative",
                colorDesc(ku::ImageExtentDesc::swapchainRelative()));
            relativeImage2 = builder.createImage(
                "Relative2",
                colorDesc(ku::ImageExtentDesc::swapchainRelative()));
            buffer = builder.createBuffer("Buffer", bufferDesc());
            builder.useImage(absoluteImage, ku::ImageUse::TransferDestination);
            builder.useImage(relativeImage, ku::ImageUse::TransferDestination);
            builder.useImage(relativeImage2, ku::ImageUse::TransferDestination);
            builder.useBuffer(buffer, ku::BufferUse::TransferDestination);
        });
        setupPass(graph, pass);
        graph.compile();
    }
};

ku::CompiledResourceUse imageUse(
    uint32_t index,
    ku::ImageUse kind,
    ku::ResourceState2 state,
    bool reads,
    bool writes,
    ku::ImageSubresourceRange range = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1})
{
    ku::CompiledResourceUse use{};
    use.resource = {index, 1, ku::ResourceKind::Image};
    use.imageUse = kind;
    use.state = state;
    use.imageRange = range;
    use.reads = reads;
    use.writes = writes;
    return use;
}

ku::CompiledResourceUse bufferUse(
    uint32_t index,
    ku::BufferUse kind,
    ku::ResourceState2 state,
    bool reads,
    bool writes,
    ku::BufferRange range)
{
    ku::CompiledResourceUse use{};
    use.resource = {index, 1, ku::ResourceKind::Buffer};
    use.bufferUse = kind;
    use.state = state;
    use.bufferRange = range;
    use.reads = reads;
    use.writes = writes;
    return use;
}

} // namespace

TEST(RenderGraphStatePlannerTest, PlansSameLayoutRawWarWawAndAccumulatesReaders)
{
    const ku::ResourceState2 writeState{
        VK_PIPELINE_STAGE_2_TRANSFER_BIT,
        VK_ACCESS_2_TRANSFER_WRITE_BIT,
        VK_IMAGE_LAYOUT_GENERAL};
    const ku::ResourceState2 fragmentRead{
        VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
        VK_ACCESS_2_SHADER_STORAGE_READ_BIT,
        VK_IMAGE_LAYOUT_GENERAL};
    const ku::ResourceState2 transferRead{
        VK_PIPELINE_STAGE_2_TRANSFER_BIT,
        VK_ACCESS_2_TRANSFER_READ_BIT,
        VK_IMAGE_LAYOUT_GENERAL};
    const ku::StatePlannerResource resource{
        {0, 1, ku::ResourceKind::Image}, "Image", {}, false,
        {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}, {}};
    const auto plan = ku::RenderGraphStatePlanner::plan(
        {resource},
        {
            {0, "Write", true, {{imageUse(0, ku::ImageUse::FragmentStorageWrite, writeState, false, true), false}}},
            {1, "ReadA", true, {{imageUse(0, ku::ImageUse::FragmentStorageRead, fragmentRead, true, false), false}}},
            {2, "ReadB", true, {{imageUse(0, ku::ImageUse::TransferSource, transferRead, true, false), false}}},
            {3, "WriteAfterReads", true, {{imageUse(0, ku::ImageUse::FragmentStorageWrite, writeState, false, true), false}}},
            {4, "WriteAgain", true, {{imageUse(0, ku::ImageUse::FragmentStorageWrite, writeState, false, true), false}}},
        });
    ASSERT_EQ(plan.barriersBeforePass[1].size(), 1u);
    EXPECT_EQ(plan.barriersBeforePass[1][0].hazard,
        ku::ResourceHazardType::ReadAfterWrite);
    EXPECT_EQ(plan.barriersBeforePass[1][0].after.stages,
        VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT);
    ASSERT_EQ(plan.barriersBeforePass[2].size(), 1u);
    EXPECT_EQ(plan.barriersBeforePass[2][0].hazard,
        ku::ResourceHazardType::ReadAfterWrite);
    EXPECT_EQ(plan.barriersBeforePass[2][0].before.stages,
        VK_PIPELINE_STAGE_2_TRANSFER_BIT);
    EXPECT_EQ(plan.barriersBeforePass[2][0].after.stages,
        VK_PIPELINE_STAGE_2_TRANSFER_BIT);
    ASSERT_EQ(plan.barriersBeforePass[3].size(), 1u);
    EXPECT_EQ(plan.barriersBeforePass[3][0].hazard,
        ku::ResourceHazardType::WriteAfterRead);
    EXPECT_EQ(
        plan.barriersBeforePass[3][0].before.stages,
        VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT
            | VK_PIPELINE_STAGE_2_TRANSFER_BIT);
    ASSERT_EQ(plan.barriersBeforePass[4].size(), 1u);
    EXPECT_EQ(plan.barriersBeforePass[4][0].hazard,
        ku::ResourceHazardType::WriteAfterWrite);
}

TEST(RenderGraphStatePlannerTest,
    TracksCoveredReadStageAccessPairsWithoutCartesianExpansion)
{
    const ku::ResourceState2 write{
        VK_PIPELINE_STAGE_2_COPY_BIT,
        VK_ACCESS_2_TRANSFER_WRITE_BIT,
        VK_IMAGE_LAYOUT_UNDEFINED};
    const ku::ResourceState2 vertexUniform{
        VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT,
        VK_ACCESS_2_UNIFORM_READ_BIT,
        VK_IMAGE_LAYOUT_UNDEFINED};
    const ku::ResourceState2 fragmentStorage{
        VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
        VK_ACCESS_2_SHADER_STORAGE_READ_BIT,
        VK_IMAGE_LAYOUT_UNDEFINED};
    const ku::ResourceState2 fragmentUniform{
        VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
        VK_ACCESS_2_UNIFORM_READ_BIT,
        VK_IMAGE_LAYOUT_UNDEFINED};
    const ku::StatePlannerResource resource{
        {0, 1, ku::ResourceKind::Buffer}, "Pairs", {}, false, {}, {0, 128}};

    const auto plan = ku::RenderGraphStatePlanner::plan(
        {resource},
        {
            {0, "Write", true,
             {{bufferUse(0, ku::BufferUse::TransferDestination,
                 write, false, true, {0, 128}), false}}},
            {1, "VertexUniform", true,
             {{bufferUse(0, ku::BufferUse::VertexUniform,
                 vertexUniform, true, false, {0, 128}), false}}},
            {2, "FragmentStorage", true,
             {{bufferUse(0, ku::BufferUse::FragmentStorageRead,
                 fragmentStorage, true, false, {0, 128}), false}}},
            {3, "FragmentUniform", true,
             {{bufferUse(0, ku::BufferUse::FragmentUniform,
                 fragmentUniform, true, false, {0, 128}), false}}},
            {4, "FragmentUniformRepeat", true,
             {{bufferUse(0, ku::BufferUse::FragmentUniform,
                 fragmentUniform, true, false, {0, 128}), false}}},
        });
    ASSERT_EQ(plan.barriersBeforePass[1].size(), 1u);
    ASSERT_EQ(plan.barriersBeforePass[2].size(), 1u);
    ASSERT_EQ(plan.barriersBeforePass[3].size(), 1u);
    EXPECT_EQ(plan.barriersBeforePass[3][0].before.stages,
        VK_PIPELINE_STAGE_2_COPY_BIT);
    EXPECT_EQ(plan.barriersBeforePass[3][0].after.stages,
        VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT);
    EXPECT_EQ(plan.barriersBeforePass[3][0].after.access,
        VK_ACCESS_2_UNIFORM_READ_BIT);
    EXPECT_TRUE(plan.barriersBeforePass[4].empty());

    const auto reversePlan = ku::RenderGraphStatePlanner::plan(
        {resource},
        {
            {0, "Write", true,
             {{bufferUse(0, ku::BufferUse::TransferDestination,
                 write, false, true, {0, 128}), false}}},
            {1, "FragmentUniform", true,
             {{bufferUse(0, ku::BufferUse::FragmentUniform,
                 fragmentUniform, true, false, {0, 128}), false}}},
            {2, "FragmentStorage", true,
             {{bufferUse(0, ku::BufferUse::FragmentStorageRead,
                 fragmentStorage, true, false, {0, 128}), false}}},
            {3, "VertexUniform", true,
             {{bufferUse(0, ku::BufferUse::VertexUniform,
                 vertexUniform, true, false, {0, 128}), false}}},
            {4, "VertexUniformRepeat", true,
             {{bufferUse(0, ku::BufferUse::VertexUniform,
                 vertexUniform, true, false, {0, 128}), false}}},
        });
    ASSERT_EQ(reversePlan.barriersBeforePass[1].size(), 1u);
    ASSERT_EQ(reversePlan.barriersBeforePass[2].size(), 1u);
    ASSERT_EQ(reversePlan.barriersBeforePass[3].size(), 1u);
    EXPECT_TRUE(reversePlan.barriersBeforePass[4].empty());

    const ku::ResourceState2 multiStageUniform{
        VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT
            | VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
        VK_ACCESS_2_UNIFORM_READ_BIT,
        VK_IMAGE_LAYOUT_UNDEFINED};
    const auto multiStagePlan = ku::RenderGraphStatePlanner::plan(
        {resource},
        {
            {0, "Write", true,
             {{bufferUse(0, ku::BufferUse::TransferDestination,
                 write, false, true, {0, 128}), false}}},
            {1, "MultiStageUniform", true,
             {{bufferUse(0, ku::BufferUse::VertexUniform,
                 multiStageUniform, true, false, {0, 128}), false}}},
            {2, "VertexUniform", true,
             {{bufferUse(0, ku::BufferUse::VertexUniform,
                 vertexUniform, true, false, {0, 128}), false}}},
            {3, "FragmentUniform", true,
             {{bufferUse(0, ku::BufferUse::FragmentUniform,
                 fragmentUniform, true, false, {0, 128}), false}}},
        });
    ASSERT_EQ(multiStagePlan.barriersBeforePass[1].size(), 1u);
    EXPECT_TRUE(multiStagePlan.barriersBeforePass[2].empty());
    EXPECT_TRUE(multiStagePlan.barriersBeforePass[3].empty());

    const auto writerResetPlan = ku::RenderGraphStatePlanner::plan(
        {resource},
        {
            {0, "WriteA", true,
             {{bufferUse(0, ku::BufferUse::TransferDestination,
                 write, false, true, {0, 128}), false}}},
            {1, "FragmentUniformA", true,
             {{bufferUse(0, ku::BufferUse::FragmentUniform,
                 fragmentUniform, true, false, {0, 128}), false}}},
            {2, "WriteB", true,
             {{bufferUse(0, ku::BufferUse::TransferDestination,
                 write, false, true, {0, 128}), false}}},
            {3, "FragmentUniformB", true,
             {{bufferUse(0, ku::BufferUse::FragmentUniform,
                 fragmentUniform, true, false, {0, 128}), false}}},
        });
    ASSERT_EQ(writerResetPlan.barriersBeforePass[1].size(), 1u);
    ASSERT_EQ(writerResetPlan.barriersBeforePass[2].size(), 1u);
    EXPECT_EQ(writerResetPlan.barriersBeforePass[2][0].hazard,
        ku::ResourceHazardType::WriteAfterRead);
    ASSERT_EQ(writerResetPlan.barriersBeforePass[3].size(), 1u);
    EXPECT_EQ(writerResetPlan.barriersBeforePass[3][0].hazard,
        ku::ResourceHazardType::ReadAfterWrite);
}

TEST(RenderGraphStatePlannerTest, ReadReadLayoutChangeBarriersWithoutHazard)
{
    const ku::ResourceState2 sampled{
        VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
        VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
    const ku::ResourceState2 transfer{
        VK_PIPELINE_STAGE_2_TRANSFER_BIT,
        VK_ACCESS_2_TRANSFER_READ_BIT,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL};
    const auto plan = ku::RenderGraphStatePlanner::plan(
        {{{0, 1, ku::ResourceKind::Image}, "Image", sampled, true,
          {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}, {}}},
        {
            {0, "Sample", true, {{imageUse(0, ku::ImageUse::FragmentSampled, sampled, true, false), false}}},
            {1, "Copy", true, {{imageUse(0, ku::ImageUse::TransferSource, transfer, true, false), false}}},
        });
    ASSERT_EQ(plan.barriersBeforePass[1].size(), 1u);
    EXPECT_FALSE(plan.barriersBeforePass[1][0].hasHazard);
    EXPECT_EQ(plan.barriersBeforePass[1][0].before.layout,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    EXPECT_EQ(plan.barriersBeforePass[1][0].after.layout,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

    const ku::ResourceState2 write{
        VK_PIPELINE_STAGE_2_TRANSFER_BIT,
        VK_ACCESS_2_TRANSFER_WRITE_BIT,
        VK_IMAGE_LAYOUT_GENERAL};
    const auto writtenPlan = ku::RenderGraphStatePlanner::plan(
        {{{0, 1, ku::ResourceKind::Image}, "WrittenImage", write, true,
          {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}, {}}},
        {
            {0, "Sample", true,
             {{imageUse(0, ku::ImageUse::FragmentSampled,
                 sampled, true, false), false}}},
            {1, "Copy", true,
             {{imageUse(0, ku::ImageUse::TransferSource,
                 transfer, true, false), false}}},
        });
    ASSERT_EQ(writtenPlan.barriersBeforePass[0].size(), 1u);
    ASSERT_EQ(writtenPlan.barriersBeforePass[1].size(), 1u);
    EXPECT_EQ(writtenPlan.barriersBeforePass[0][0].hazard,
        ku::ResourceHazardType::ReadAfterWrite);
    EXPECT_EQ(writtenPlan.barriersBeforePass[1][0].hazard,
        ku::ResourceHazardType::ReadAfterWrite);
    EXPECT_EQ(writtenPlan.barriersBeforePass[1][0].before.layout,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    EXPECT_EQ(writtenPlan.barriersBeforePass[1][0].after.layout,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
}

TEST(RenderGraphStatePlannerTest, BufferRangesBarrierOnlyOverlappingSegments)
{
    const ku::ResourceState2 write{
        VK_PIPELINE_STAGE_2_TRANSFER_BIT,
        VK_ACCESS_2_TRANSFER_WRITE_BIT,
        VK_IMAGE_LAYOUT_UNDEFINED};
    const ku::ResourceState2 read{
        VK_PIPELINE_STAGE_2_VERTEX_ATTRIBUTE_INPUT_BIT,
        VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT,
        VK_IMAGE_LAYOUT_UNDEFINED};
    const auto plan = ku::RenderGraphStatePlanner::plan(
        {{{0, 1, ku::ResourceKind::Buffer}, "Buffer", {}, true, {}, {0, 128}}},
        {
            {0, "WriteLeft", true, {{bufferUse(0, ku::BufferUse::TransferDestination, write, false, true, {0, 64}), false}}},
            {1, "ReadRight", true, {{bufferUse(0, ku::BufferUse::Vertex, read, true, false, {64, 64}), false}}},
            {2, "ReadOverlap", true, {{bufferUse(0, ku::BufferUse::Vertex, read, true, false, {32, 64}), false}}},
        });
    EXPECT_TRUE(plan.barriersBeforePass[1].empty());
    ASSERT_EQ(plan.barriersBeforePass[2].size(), 1u);
    EXPECT_EQ(plan.barriersBeforePass[2][0].bufferRange.offset, 32u);
    EXPECT_EQ(plan.barriersBeforePass[2][0].bufferRange.size, 32u);
}

TEST(RenderGraphStatePlannerTest,
    ReadLookAheadRespectsRangeLayoutAndEnabledMask)
{
    const ku::ResourceState2 write{
        VK_PIPELINE_STAGE_2_TRANSFER_BIT,
        VK_ACCESS_2_TRANSFER_WRITE_BIT,
        VK_IMAGE_LAYOUT_UNDEFINED};
    const ku::ResourceState2 vertex{
        VK_PIPELINE_STAGE_2_VERTEX_ATTRIBUTE_INPUT_BIT,
        VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT,
        VK_IMAGE_LAYOUT_UNDEFINED};
    const ku::ResourceState2 index{
        VK_PIPELINE_STAGE_2_INDEX_INPUT_BIT,
        VK_ACCESS_2_INDEX_READ_BIT,
        VK_IMAGE_LAYOUT_UNDEFINED};
    const ku::ResourceState2 indirect{
        VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT,
        VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT,
        VK_IMAGE_LAYOUT_UNDEFINED};
    const ku::ResourceState2 disabledRead{
        VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
        VK_ACCESS_2_SHADER_STORAGE_READ_BIT,
        VK_IMAGE_LAYOUT_UNDEFINED};
    const ku::ResourceState2 transferBufferRead{
        VK_PIPELINE_STAGE_2_COPY_BIT,
        VK_ACCESS_2_TRANSFER_READ_BIT,
        VK_IMAGE_LAYOUT_UNDEFINED};
    const auto bufferPlan = ku::RenderGraphStatePlanner::plan(
        {{{0, 1, ku::ResourceKind::Buffer}, "Buffer", write, true, {}, {0, 128}}},
        {
            {0, "Draw", true,
             {
                 {bufferUse(0, ku::BufferUse::Vertex, vertex, true, false, {0, 64}), false},
                 {bufferUse(0, ku::BufferUse::Index, index, true, false, {0, 32}), false},
                 {bufferUse(0, ku::BufferUse::Indirect, indirect, true, false, {64, 64}), false},
             }},
            {1, "Disabled", false,
             {{bufferUse(0, ku::BufferUse::FragmentStorageRead,
                 disabledRead, true, false, {0, 128}), false}}},
        });
    ASSERT_EQ(bufferPlan.barriersBeforePass[0].size(), 2u);
    const auto left = std::find_if(
        bufferPlan.barriersBeforePass[0].begin(),
        bufferPlan.barriersBeforePass[0].end(),
        [](const ku::PlannedResourceBarrier& barrier) {
            return barrier.bufferRange.offset == 0;
        });
    ASSERT_NE(left, bufferPlan.barriersBeforePass[0].end());
    EXPECT_EQ(left->bufferRange.size, 64u);
    EXPECT_EQ(left->after.stages,
        VK_PIPELINE_STAGE_2_VERTEX_ATTRIBUTE_INPUT_BIT
            | VK_PIPELINE_STAGE_2_INDEX_INPUT_BIT);
    EXPECT_EQ(left->after.stages & VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT, 0u);
    EXPECT_EQ(left->after.stages & VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, 0u);
    EXPECT_TRUE(bufferPlan.barriersBeforePass[1].empty());

    const auto crossPassPlan = ku::RenderGraphStatePlanner::plan(
        {{{0, 1, ku::ResourceKind::Buffer}, "CrossPass", {}, false, {}, {0, 128}}},
        {
            {0, "Write", true,
             {{bufferUse(0, ku::BufferUse::TransferDestination,
                 write, false, true, {0, 128}), false}}},
            {1, "Vertex", true,
             {{bufferUse(0, ku::BufferUse::Vertex,
                 vertex, true, false, {0, 64}), false}}},
            {2, "DisabledFragment", false,
             {{bufferUse(0, ku::BufferUse::FragmentStorageRead,
                 disabledRead, true, false, {0, 64}), false}}},
            {3, "Index", true,
             {{bufferUse(0, ku::BufferUse::Index,
                 index, true, false, {0, 64}), false}}},
            {4, "IndirectOtherRange", true,
             {{bufferUse(0, ku::BufferUse::Indirect,
                 indirect, true, false, {64, 64}), false}}},
            {5, "TransferLeft", true,
             {{bufferUse(0, ku::BufferUse::TransferSource,
                 transferBufferRead, true, false, {0, 64}), false}}},
            {6, "WriteAfterLeftReaders", true,
             {{bufferUse(0, ku::BufferUse::TransferDestination,
                 write, false, true, {0, 64}), false}}},
        });
    ASSERT_EQ(crossPassPlan.barriersBeforePass[1].size(), 1u);
    EXPECT_EQ(crossPassPlan.barriersBeforePass[1][0].after.stages,
        VK_PIPELINE_STAGE_2_VERTEX_ATTRIBUTE_INPUT_BIT);
    EXPECT_TRUE(crossPassPlan.barriersBeforePass[2].empty());
    ASSERT_EQ(crossPassPlan.barriersBeforePass[3].size(), 1u);
    EXPECT_EQ(crossPassPlan.barriersBeforePass[3][0].after.stages,
        VK_PIPELINE_STAGE_2_INDEX_INPUT_BIT);
    EXPECT_EQ(crossPassPlan.barriersBeforePass[3][0].after.stages
            & VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
        0u);
    ASSERT_EQ(crossPassPlan.barriersBeforePass[4].size(), 1u);
    EXPECT_EQ(crossPassPlan.barriersBeforePass[4][0].bufferRange.offset, 64u);
    EXPECT_EQ(crossPassPlan.barriersBeforePass[4][0].bufferRange.size, 64u);
    ASSERT_EQ(crossPassPlan.barriersBeforePass[5].size(), 1u);
    EXPECT_EQ(crossPassPlan.barriersBeforePass[5][0].hazard,
        ku::ResourceHazardType::ReadAfterWrite);
    EXPECT_EQ(crossPassPlan.barriersBeforePass[5][0].after.stages,
        VK_PIPELINE_STAGE_2_COPY_BIT);
    ASSERT_EQ(crossPassPlan.barriersBeforePass[6].size(), 1u);
    EXPECT_EQ(crossPassPlan.barriersBeforePass[6][0].hazard,
        ku::ResourceHazardType::WriteAfterRead);
    EXPECT_EQ(crossPassPlan.barriersBeforePass[6][0].before.stages,
        VK_PIPELINE_STAGE_2_VERTEX_ATTRIBUTE_INPUT_BIT
            | VK_PIPELINE_STAGE_2_INDEX_INPUT_BIT
            | VK_PIPELINE_STAGE_2_COPY_BIT);

    const ku::ResourceState2 imageWrite{
        VK_PIPELINE_STAGE_2_TRANSFER_BIT,
        VK_ACCESS_2_TRANSFER_WRITE_BIT,
        VK_IMAGE_LAYOUT_GENERAL};
    const ku::ResourceState2 sampled{
        VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
        VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
    const ku::ResourceState2 transferRead{
        VK_PIPELINE_STAGE_2_COPY_BIT,
        VK_ACCESS_2_TRANSFER_READ_BIT,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL};
    const auto imagePlan = ku::RenderGraphStatePlanner::plan(
        {{{0, 1, ku::ResourceKind::Image}, "Image", imageWrite, true,
          {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}, {}}},
        {{0, "DifferentLayouts", true,
          {
              {imageUse(0, ku::ImageUse::FragmentSampled,
                   sampled, true, false), false},
              {imageUse(0, ku::ImageUse::TransferSource,
                   transferRead, true, false), false},
          }}});
    ASSERT_EQ(imagePlan.barriersBeforePass[0].size(), 2u);
    EXPECT_EQ(imagePlan.barriersBeforePass[0][0].after.stages,
        VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT);
    EXPECT_EQ(imagePlan.barriersBeforePass[0][1].after.stages,
        VK_PIPELINE_STAGE_2_COPY_BIT);
}

TEST(RenderGraphStatePlannerTest, DisabledOrDiscardedProducerFailsPreflight)
{
    const ku::ResourceState2 write{
        VK_PIPELINE_STAGE_2_TRANSFER_BIT,
        VK_ACCESS_2_TRANSFER_WRITE_BIT,
        VK_IMAGE_LAYOUT_UNDEFINED};
    const ku::ResourceState2 read{
        VK_PIPELINE_STAGE_2_VERTEX_INPUT_BIT,
        VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT,
        VK_IMAGE_LAYOUT_UNDEFINED};
    const ku::StatePlannerResource resource{
        {0, 1, ku::ResourceKind::Buffer}, "Buffer", {}, false, {}, {0, 64}};
    try {
        (void)ku::RenderGraphStatePlanner::plan(
            {resource},
            {
                {0, "SkippedProducer", false, {{bufferUse(0, ku::BufferUse::TransferDestination, write, false, true, {0, 64}), false}}},
                {1, "Consumer", true, {{bufferUse(0, ku::BufferUse::Vertex, read, true, false, {0, 64}), false}}},
            });
        FAIL() << "expected disabled producer diagnostic";
    } catch (const std::runtime_error& error) {
        EXPECT_NE(std::string(error.what()).find("SkippedProducer"), std::string::npos);
        EXPECT_NE(std::string(error.what()).find("Consumer"), std::string::npos);
        EXPECT_NE(std::string(error.what()).find("Buffer"), std::string::npos);
    }
    EXPECT_THROW(
        (void)ku::RenderGraphStatePlanner::plan(
            {resource},
            {
                {0, "Discard", true, {{bufferUse(0, ku::BufferUse::TransferDestination, write, false, true, {0, 64}), true}}},
                {1, "Read", true, {{bufferUse(0, ku::BufferUse::Vertex, read, true, false, {0, 64}), false}}},
            }),
        std::runtime_error);

    const ku::StatePlannerResource rangedResource{
        {0, 1, ku::ResourceKind::Buffer}, "RangedBuffer", {}, false, {}, {0, 128}};
    try {
        (void)ku::RenderGraphStatePlanner::plan(
            {rangedResource},
            {
                {0, "SkippedLeft", false,
                 {{bufferUse(0, ku::BufferUse::TransferDestination,
                     write, false, true, {0, 64}), false}}},
                {1, "ReadRight", true,
                 {{bufferUse(0, ku::BufferUse::Vertex,
                     read, true, false, {64, 64}), false}}},
            });
        FAIL() << "expected invalid right-range contents";
    } catch (const std::runtime_error& error) {
        EXPECT_EQ(std::string(error.what()).find("SkippedLeft"),
            std::string::npos);
    }
    try {
        (void)ku::RenderGraphStatePlanner::plan(
            {rangedResource},
            {
                {0, "SkippedLeft", false,
                 {{bufferUse(0, ku::BufferUse::TransferDestination,
                     write, false, true, {0, 64}), false}}},
                {1, "ReadLeft", true,
                 {{bufferUse(0, ku::BufferUse::Vertex,
                     read, true, false, {0, 64}), false}}},
            });
        FAIL() << "expected skipped left-range producer diagnostic";
    } catch (const std::runtime_error& error) {
        EXPECT_NE(std::string(error.what()).find("SkippedLeft"),
            std::string::npos);
    }
}

TEST(RenderGraphStatePlannerTest, RequiresSynchronization2AndDynamicRendering)
{
    VkPhysicalDeviceVulkan13Features features{
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};
    features.dynamicRendering = VK_TRUE;
    features.synchronization2 = VK_TRUE;
    EXPECT_TRUE(ku::supportsRequiredVulkan13Features(features));
    features.synchronization2 = VK_FALSE;
    EXPECT_FALSE(ku::supportsRequiredVulkan13Features(features));
    features.synchronization2 = VK_TRUE;
    features.dynamicRendering = VK_FALSE;
    EXPECT_FALSE(ku::supportsRequiredVulkan13Features(features));
}

TEST(RenderGraphStatePlannerTest, RetainsPhysicalHazardWhenFrameContentIsInvalid)
{
    const ku::ResourceState2 attachmentWrite{
        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    const auto plan = ku::RenderGraphStatePlanner::plan(
        {{{0, 1, ku::ResourceKind::Image}, "Image", attachmentWrite, false,
          {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}, {}}},
        {{0, "ClearAgain", true,
          {{imageUse(0, ku::ImageUse::ColorAttachment,
              attachmentWrite, false, true), false}}}});
    ASSERT_EQ(plan.barriersBeforePass[0].size(), 1u);
    EXPECT_EQ(plan.barriersBeforePass[0][0].hazard,
        ku::ResourceHazardType::WriteAfterWrite);
}

TEST(RenderGraphTest, ExplicitDependencyAffectsExecutionOrder)
{
    ku::RenderGraph graph;
    TestPass passB("PassB");
    TestPass passA("PassA", [](ku::RenderGraphBuilder& builder) {
        builder.dependsOn("PassB");
    });
    const size_t passAIndex = graph.registerPass(passA);
    const size_t passBIndex = graph.registerPass(passB);
    auto setupA = graph.buildPass(passAIndex); passA.setup(setupA);
    auto setupB = graph.buildPass(passBIndex); passB.setup(setupB);
    graph.compile();
    EXPECT_EQ(graph.executionOrder(), (std::vector<size_t>{passBIndex, passAIndex}));
}

TEST(RenderGraphTest, BuildsTypedDependenciesAndBarrierPlan)
{
    ku::RenderGraph graph;
    TestPass geometry("Geometry", [](ku::RenderGraphBuilder& builder) {
        builder.useImage(builder.createImage("Color", colorDesc()), ku::ImageUse::TransferDestination);
    });
    TestPass lighting("Lighting", [](ku::RenderGraphBuilder& builder) {
        builder.useImage(builder.createImage("Color", colorDesc()), ku::ImageUse::FragmentSampled);
        builder.useImage(builder.createImage("Lit", colorDesc()), ku::ImageUse::TransferDestination);
    });
    TestPass toneMap("ToneMap", [](ku::RenderGraphBuilder& builder) {
        builder.useImage(builder.createImage("Lit", colorDesc()), ku::ImageUse::FragmentSampled);
        builder.useImage(builder.importImage("Swap", colorDesc()), ku::ImageUse::TransferDestination);
    });
    setupPass(graph, geometry);
    setupPass(graph, lighting);
    setupPass(graph, toneMap);
    graph.compile();
    EXPECT_EQ(graph.dependencies().size(), 2u);
    EXPECT_EQ(graph.barrierPlan().size(), 2u);
}

TEST(RenderGraphTest, GeneratesAllHazardTypes)
{
    ku::RenderGraph graph;
    TestPass a("A", [](ku::RenderGraphBuilder& b) {
        b.useBuffer(b.createBuffer("Shared", bufferDesc()), ku::BufferUse::TransferDestination);
    });
    TestPass b("B", [](ku::RenderGraphBuilder& v) {
        v.useBuffer(v.createBuffer("Shared", bufferDesc()), ku::BufferUse::FragmentStorageRead);
    });
    TestPass c("C", [](ku::RenderGraphBuilder& b) {
        b.useBuffer(b.createBuffer("Shared", bufferDesc()), ku::BufferUse::TransferDestination);
    });
    setupPass(graph, a); setupPass(graph, b); setupPass(graph, c);
    graph.compile();
    ASSERT_EQ(graph.barrierPlan().size(), 3u);
    EXPECT_EQ(graph.barrierPlan()[0].hazard, ku::ResourceHazardType::ReadAfterWrite);
    EXPECT_EQ(graph.barrierPlan()[1].hazard, ku::ResourceHazardType::WriteAfterWrite);
    EXPECT_EQ(graph.barrierPlan()[2].hazard, ku::ResourceHazardType::WriteAfterRead);
}

TEST(RenderGraphTest, RejectsMissingDependencyCycleEmptyAndDuplicatePassNames)
{
    {
        ku::RenderGraph graph;
        TestPass pass("A", [](ku::RenderGraphBuilder& b) { b.dependsOn("Missing"); });
        setupPass(graph, pass);
        EXPECT_THROW(graph.compile(), std::runtime_error);
    }
    {
        ku::RenderGraph graph;
        TestPass a("A", [](ku::RenderGraphBuilder& b) { b.dependsOn("B"); });
        TestPass b("B", [](ku::RenderGraphBuilder& v) { v.dependsOn("A"); });
        setupPass(graph, a); setupPass(graph, b);
        EXPECT_THROW(graph.compile(), std::runtime_error);
    }
    for (const auto& names : {std::pair{"", "B"}, std::pair{"A", "A"}}) {
        ku::RenderGraph graph;
        TestPass a(names.first), b(names.second);
        setupPass(graph, a); setupPass(graph, b);
        EXPECT_THROW(graph.compile(), std::runtime_error);
    }
}

TEST(RenderGraphTest, SameNameRequiresExactKindOwnershipAndDescription)
{
    ku::RenderGraph graph;
    TestPass pass("Declare");
    const size_t passIndex = graph.registerPass(pass);
    auto builder = graph.buildPass(passIndex);
    const ku::ImageHandle first = builder.createImage("Same", colorDesc());
    EXPECT_EQ(first, builder.createImage("Same", colorDesc()));
    EXPECT_THROW((void)builder.importImage("Same", colorDesc()), std::runtime_error);
    EXPECT_THROW((void)builder.createBuffer("Same", bufferDesc()), std::runtime_error);
    auto changed = colorDesc(); changed.format = VK_FORMAT_B8G8R8A8_UNORM;
    EXPECT_THROW((void)builder.createImage("Same", changed), std::runtime_error);
    builder.useImage(first, ku::ImageUse::TransferDestination);
    graph.compile();
}

TEST(RenderGraphTest, DiagnosesInvalidStaleAndWrongKindHandles)
{
    ku::RenderGraph graph;
    TestPass pass("Handles");
    const size_t passIndex = graph.registerPass(pass);
    auto builder = graph.buildPass(passIndex);
    EXPECT_THROW(builder.useImage(ku::ImageHandle{}, ku::ImageUse::FragmentSampled), std::invalid_argument);
    const ku::BufferHandle buffer = builder.createBuffer("B", bufferDesc());
    const ku::ImageHandle wrongKind{buffer.index, buffer.graphGeneration};
    EXPECT_THROW((void)graph.resource(wrongKind), std::invalid_argument);
    builder.useBuffer(buffer, ku::BufferUse::TransferDestination);
    const ku::BufferHandle stale = buffer;
    graph.reset();
    EXPECT_THROW((void)graph.resource(stale), std::invalid_argument);
}

TEST(RenderGraphTest, ValidatesDescriptionsFirstUseAttachmentsAndExports)
{
    auto expectCompileFailure = [](const TestPass::SetupFn& setup) {
        ku::RenderGraph graph;
        TestPass pass("Invalid", setup);
        EXPECT_THROW({
            setupPass(graph, pass);
            graph.compile();
        }, std::exception);
    };
    expectCompileFailure([](ku::RenderGraphBuilder& b) {
        auto desc = colorDesc(); desc.format = VK_FORMAT_UNDEFINED;
        b.useImage(b.createImage("Bad", desc), ku::ImageUse::TransferDestination);
    });
    expectCompileFailure([](ku::RenderGraphBuilder& b) {
        auto desc = colorDesc(); desc.mipLevels = 2;
        b.useImage(b.createImage("Bad", desc), ku::ImageUse::TransferDestination);
    });
    expectCompileFailure([](ku::RenderGraphBuilder& b) {
        b.useBuffer(b.createBuffer("UndefinedRead", bufferDesc()), ku::BufferUse::FragmentStorageRead);
    });
    expectCompileFailure([](ku::RenderGraphBuilder& b) {
        b.useImage(b.createImage(
            "ImplicitClearCannotFeedRead",
            colorDesc(
                ku::ImageExtentDesc::absoluteExtent(64, 64),
                ku::InitialContent::Cleared)), ku::ImageUse::FragmentSampled);
    });
    expectCompileFailure([](ku::RenderGraphBuilder& b) {
        b.useBuffer(b.createBuffer(
            "UnsupportedBufferClear",
            bufferDesc(ku::InitialContent::Cleared)), ku::BufferUse::TransferDestination);
    });
    expectCompileFailure([](ku::RenderGraphBuilder& b) {
        auto image = b.createImage("Load", colorDesc());
        b.colorAttachment(image, ku::AttachmentLoadPolicy::Load);
    });
    expectCompileFailure([](ku::RenderGraphBuilder& b) {
        auto image = b.createImage("ColorAsDepth", colorDesc());
        b.depthAttachment(image);
    });
    expectCompileFailure([](ku::RenderGraphBuilder& b) {
        auto image = b.createImage("ReadWrite", colorDesc());
        b.useImage(image, ku::ImageUse::FragmentSampled);
        b.useImage(image, ku::ImageUse::TransferDestination);
    });
    expectCompileFailure([](ku::RenderGraphBuilder& b) {
        (void)b.createBuffer("Unused", bufferDesc());
    });
    expectCompileFailure([](ku::RenderGraphBuilder& b) {
        auto image = b.createImage("NoStore", colorDesc());
        b.colorAttachment(image, ku::AttachmentLoadPolicy::Clear,
            ku::AttachmentStorePolicy::DontCare);
        b.exportImage(image);
    });
}

TEST(RenderGraphTest, AttachmentDeclarationAddsTypedWriteAccess)
{
    ku::RenderGraph graph;
    TestPass pass("Color", [](ku::RenderGraphBuilder& b) {
        b.colorAttachment(
            b.importImage("Swap", colorDesc()),
            ku::AttachmentLoadPolicy::Clear,
            ku::AttachmentStorePolicy::Store);
    });
    setupPass(graph, pass);
    graph.compile();
    ASSERT_EQ(graph.passes()[0].attachments.size(), 1u);
    ASSERT_EQ(graph.passes()[0].uses.size(), 1u);
    EXPECT_TRUE(graph.passes()[0].uses[0].writes);
    EXPECT_EQ(graph.passes()[0].uses[0].imageUse, ku::ImageUse::ColorAttachment);
    EXPECT_TRUE(graph.resources()[0].external());
}

TEST(RenderGraphTest, RejectsDuplicateAndMultipleDepthAttachments)
{
    ku::RenderGraph graph;
    TestPass pass("Depth");
    const size_t index = graph.registerPass(pass);
    auto builder = graph.buildPass(index);
    const auto a = builder.importImage("DepthA", depthDesc());
    const auto b = builder.importImage("DepthB", depthDesc());
    builder.depthAttachment(a);
    EXPECT_THROW(builder.depthAttachment(a), std::runtime_error);
    EXPECT_THROW(builder.depthAttachment(b), std::runtime_error);
}

TEST(RenderGraphTest, NormalizesAndValidatesRangesAndUsage)
{
    const ku::BufferDesc desc = bufferDesc();
    EXPECT_EQ(
        ku::RenderGraph::normalizeBufferRange(desc, {64, VK_WHOLE_SIZE}),
        (ku::BufferRange{64, 192}));
    EXPECT_THROW(
        (void)ku::RenderGraph::normalizeBufferRange(desc, {256, VK_WHOLE_SIZE}),
        std::out_of_range);
    EXPECT_THROW(
        (void)ku::RenderGraph::normalizeBufferRange(desc, {250, 16}),
        std::out_of_range);
    EXPECT_FALSE(ku::RenderGraph::overlaps(
        ku::BufferRange{0, 64}, ku::BufferRange{64, 64}));
    EXPECT_TRUE(ku::RenderGraph::overlaps(
        ku::BufferRange{0, 65}, ku::BufferRange{64, 64}));

    ku::RenderGraph graph;
    TestPass pass("Usage");
    const size_t index = graph.registerPass(pass);
    auto builder = graph.buildPass(index);
    ku::BufferDesc transferOnly = bufferDesc();
    transferOnly.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    const auto buffer = builder.createBuffer("TransferOnly", transferOnly);
    EXPECT_THROW(
        builder.useBuffer(buffer, ku::BufferUse::Vertex),
        std::runtime_error);
    builder.useBuffer(buffer, ku::BufferUse::TransferDestination);
    EXPECT_NO_THROW(graph.compile());
}

TEST(RenderGraphTest, NonOverlappingRangesDoNotCreateDependencies)
{
    ku::RenderGraph graph;
    TestPass left("Left", [](ku::RenderGraphBuilder& builder) {
        builder.useBuffer(
            builder.createBuffer("Ranges", bufferDesc()),
            ku::BufferUse::TransferDestination,
            {0, 64});
    });
    TestPass right("Right", [](ku::RenderGraphBuilder& builder) {
        builder.useBuffer(
            builder.createBuffer("Ranges", bufferDesc()),
            ku::BufferUse::TransferDestination,
            {64, 64});
    });
    setupPass(graph, left);
    setupPass(graph, right);
    graph.compile();
    EXPECT_TRUE(graph.dependencies().empty());
    EXPECT_TRUE(graph.barrierPlan().empty());
}

TEST(RenderGraphTest, CheckedResolverDeclarationMatchesExactUse)
{
    ku::RenderGraph graph;
    TestPass initialize("Initialize", [](ku::RenderGraphBuilder& builder) {
        ku::BufferDesc desc = bufferDesc();
        desc.usage |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
        builder.useBuffer(
            builder.createBuffer("Buffer", desc),
            ku::BufferUse::TransferDestination,
            {0, 64});
    });
    TestPass pass("Resolver", [](ku::RenderGraphBuilder& builder) {
        ku::BufferDesc desc = bufferDesc();
        desc.usage |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
        builder.useBuffer(
            builder.createBuffer("Buffer", desc),
            ku::BufferUse::Vertex,
            {0, 64});
    });
    setupPass(graph, initialize);
    setupPass(graph, pass);
    graph.compile();
    const ku::PassNode& node = graph.passes().back();
    const ku::ResourceHandle handle = node.uses.front().resource;
    EXPECT_TRUE(ku::passDeclaresUse(node, handle, ku::BufferUse::Vertex));
    EXPECT_FALSE(ku::passDeclaresUse(node, handle, ku::BufferUse::Index));
    EXPECT_FALSE(ku::passDeclaresUse(
        node,
        ku::ResourceHandle{
            handle.index, handle.graphGeneration, ku::ResourceKind::Image},
        ku::ImageUse::FragmentSampled));
}

TEST(RenderGraphTest, RejectsTransferCommandsInsideAttachmentPass)
{
    ku::RenderGraph graph;
    TestPass pass("Mixed", [](ku::RenderGraphBuilder& builder) {
        builder.colorAttachment(
            builder.createImage("Color", colorDesc()),
            ku::AttachmentLoadPolicy::Clear,
            ku::AttachmentStorePolicy::Store);
        builder.useBuffer(
            builder.createBuffer("Transfer", bufferDesc()),
            ku::BufferUse::TransferDestination);
    });
    setupPass(graph, pass);
    EXPECT_THROW(graph.compile(), std::runtime_error);
}

TEST(RenderGraphTest, RejectsOverlappingImageLayoutsInsideOnePass)
{
    ku::RenderGraph graph;
    TestPass pass("MixedLayouts", [](ku::RenderGraphBuilder& builder) {
        const ku::ImageHandle image = builder.importImage(
            "Image", colorDesc(
                ku::ImageExtentDesc::absoluteExtent(64, 64),
                ku::InitialContent::Preserved));
        builder.useImage(image, ku::ImageUse::FragmentSampled);
        builder.useImage(image, ku::ImageUse::TransferSource);
    });
    setupPass(graph, pass);
    EXPECT_THROW(graph.compile(), std::runtime_error);
}

TEST(RenderGraphResourcePoolTest, CachesFramesAndRebuildsOnlyRelativeImagesOnResize)
{
    PoolFixtureGraph fixture;
    AllocationCounts counts;
    FakeAllocator allocator(counts);
    auto pool = ku::RenderGraphResourcePool::build(
        fixture.graph, {640, 480}, 1, allocator);
    EXPECT_EQ(counts.imagesCreated, 3u);
    EXPECT_EQ(counts.buffersCreated, 1u);
    const auto absoluteBefore = pool.resolveImage(fixture.absoluteImage);
    const auto relativeBefore = pool.resolveImage(fixture.relativeImage);
    const auto bufferBefore = pool.resolveBuffer(fixture.buffer);
    const ku::ResourceState2 preservedImageState{
        VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
        VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
    const ku::ResourceState2 preservedBufferState{
        VK_PIPELINE_STAGE_2_TRANSFER_BIT,
        VK_ACCESS_2_TRANSFER_WRITE_BIT,
        VK_IMAGE_LAYOUT_UNDEFINED};
    pool.setImageState(fixture.absoluteImage, preservedImageState, true);
    pool.setBufferState(fixture.buffer, preservedBufferState, true);
    for (int i = 0; i < 100; ++i) pool.invalidateTransientContents();
    EXPECT_EQ(counts.imagesCreated, 3u);
    EXPECT_EQ(counts.buffersCreated, 1u);

    auto candidate = ku::RenderGraphResourcePool::buildResizeCandidate(
        fixture.graph, {800, 600}, 2, allocator);
    EXPECT_EQ(counts.imagesCreated, 5u);
    EXPECT_EQ(counts.buffersCreated, 1u);
    candidate.adoptAbsoluteAllocationsFrom(pool);
    pool.swap(candidate);
    const auto absoluteAfter = pool.resolveImage(fixture.absoluteImage);
    const auto relativeAfter = pool.resolveImage(fixture.relativeImage);
    const auto bufferAfter = pool.resolveBuffer(fixture.buffer);
    EXPECT_EQ(absoluteAfter.image, absoluteBefore.image);
    EXPECT_EQ(absoluteAfter.allocationGeneration, absoluteBefore.allocationGeneration);
    EXPECT_NE(relativeAfter.image, relativeBefore.image);
    EXPECT_EQ(relativeAfter.allocationGeneration, 2u);
    EXPECT_EQ(relativeAfter.extent.width, 800u);
    EXPECT_EQ(relativeAfter.extent.height, 600u);
    EXPECT_EQ(bufferAfter.buffer, bufferBefore.buffer);
    EXPECT_EQ(bufferAfter.allocationGeneration, bufferBefore.allocationGeneration);
    EXPECT_EQ(absoluteAfter.state, preservedImageState);
    EXPECT_EQ(bufferAfter.state, preservedBufferState);
    EXPECT_FALSE(absoluteAfter.contentsValid);
    EXPECT_FALSE(bufferAfter.contentsValid);
}

TEST(RenderGraphResourcePoolTest, AllocationFailureRollsBackAndCleansCandidate)
{
    PoolFixtureGraph fixture;
    AllocationCounts stableCounts;
    FakeAllocator stableAllocator(stableCounts);
    auto pool = ku::RenderGraphResourcePool::build(
        fixture.graph, {640, 480}, 7, stableAllocator);
    const auto stableImage = pool.resolveImage(fixture.absoluteImage).image;
    const auto stableBuffer = pool.resolveBuffer(fixture.buffer).buffer;

    AllocationCounts imageFailureCounts;
    FakeAllocator imageFailure(imageFailureCounts);
    imageFailure.failOnCall = 2;
    EXPECT_THROW(
        (void)ku::RenderGraphResourcePool::build(
            fixture.graph, {640, 480}, 8, imageFailure),
        std::runtime_error);
    EXPECT_EQ(imageFailureCounts.imagesCreated, 1u);
    EXPECT_EQ(imageFailureCounts.imagesDestroyed, 1u);

    AllocationCounts bufferFailureCounts;
    FakeAllocator bufferFailure(bufferFailureCounts);
    bufferFailure.failOnCall = 4;
    EXPECT_THROW(
        (void)ku::RenderGraphResourcePool::build(
            fixture.graph, {640, 480}, 8, bufferFailure),
        std::runtime_error);
    EXPECT_EQ(bufferFailureCounts.imagesCreated, 3u);
    EXPECT_EQ(bufferFailureCounts.imagesDestroyed, 3u);
    EXPECT_EQ(pool.resolveImage(fixture.absoluteImage).image, stableImage);
    EXPECT_EQ(pool.resolveBuffer(fixture.buffer).buffer, stableBuffer);
}

TEST(RenderGraphResourcePoolTest, ResizeAllocationFailureLeavesPublishedPoolUntouched)
{
    PoolFixtureGraph fixture;
    AllocationCounts stableCounts;
    FakeAllocator stableAllocator(stableCounts);
    auto pool = ku::RenderGraphResourcePool::build(
        fixture.graph, {640, 480}, 11, stableAllocator);
    const auto absolute = pool.resolveImage(fixture.absoluteImage);
    const auto relative = pool.resolveImage(fixture.relativeImage);
    const auto buffer = pool.resolveBuffer(fixture.buffer);

    AllocationCounts failureCounts;
    FakeAllocator failureAllocator(failureCounts);
    failureAllocator.failOnCall = 2;
    EXPECT_THROW(
        (void)ku::RenderGraphResourcePool::buildResizeCandidate(
            fixture.graph, {800, 600}, 12, failureAllocator),
        std::runtime_error);
    EXPECT_EQ(failureCounts.imagesCreated, 1u);
    EXPECT_EQ(failureCounts.imagesDestroyed, 1u);
    EXPECT_EQ(pool.resolveImage(fixture.absoluteImage).image, absolute.image);
    EXPECT_EQ(pool.resolveImage(fixture.relativeImage).image, relative.image);
    EXPECT_EQ(pool.resolveBuffer(fixture.buffer).buffer, buffer.buffer);
}

TEST(RenderGraphResourcePoolTest, ImportedResourcesAreNeverAllocatedOrDestroyed)
{
    ku::RenderGraph graph;
    TestPass pass("External", [](ku::RenderGraphBuilder& b) {
        b.useImage(b.importImage("ExternalImage", colorDesc()), ku::ImageUse::TransferDestination);
        b.useBuffer(b.importBuffer("ExternalBuffer", bufferDesc()), ku::BufferUse::TransferDestination);
    });
    setupPass(graph, pass);
    graph.compile();
    AllocationCounts counts;
    FakeAllocator allocator(counts);
    { auto pool = ku::RenderGraphResourcePool::build(
        graph, {320, 240}, 1, allocator); (void)pool; }
    EXPECT_EQ(counts.imagesCreated, 0u);
    EXPECT_EQ(counts.imagesDestroyed, 0u);
    EXPECT_EQ(counts.buffersCreated, 0u);
    EXPECT_EQ(counts.buffersDestroyed, 0u);
}

TEST(RenderGraphExportTest, DetectsNextFrameResizeRecompileAndOwnerStaleness)
{
    auto state = std::make_shared<ku::ExportLifetimeState>();
    ku::ExportedImage image{
        .view = ku::ResolvedImage{
            .handle = {0, 1},
            .image = reinterpret_cast<VkImage>(1),
            .imageView = reinterpret_cast<VkImageView>(2),
            .extent = {64, 64},
            .format = VK_FORMAT_R8G8B8A8_UNORM,
            .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
            .aspect = VK_IMAGE_ASPECT_COLOR_BIT,
            .contentsValid = true,
        },
        .epoch = state->epoch,
        .frameSerial = state->frameSerial,
        .lifetime = state,
    };
    ku::ExportedBuffer buffer{
        .view = ku::ResolvedBuffer{
            .handle = {1, 1},
            .buffer = reinterpret_cast<VkBuffer>(3),
            .size = 256,
            .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            .contentsValid = true,
        },
        .epoch = state->epoch,
        .frameSerial = state->frameSerial,
        .lifetime = state,
    };
    EXPECT_TRUE(image.valid());
    EXPECT_TRUE(buffer.valid());
    ++state->frameSerial;
    EXPECT_FALSE(image.valid());
    EXPECT_FALSE(buffer.valid());
    image.frameSerial = state->frameSerial;
    buffer.frameSerial = state->frameSerial;
    EXPECT_TRUE(image.valid());
    EXPECT_TRUE(buffer.valid());
    ++state->epoch;
    EXPECT_FALSE(image.valid());
    EXPECT_FALSE(buffer.valid());
    image.epoch = state->epoch;
    buffer.epoch = state->epoch;
    EXPECT_TRUE(image.valid());
    EXPECT_TRUE(buffer.valid());
    state->alive = false;
    EXPECT_FALSE(image.valid());
    EXPECT_FALSE(buffer.valid());
    state.reset();
    EXPECT_FALSE(image.valid());
    EXPECT_FALSE(buffer.valid());
}

TEST(RenderGraphBindingTest, RejectsImageAndBufferDescriptorMismatches)
{
    const auto expectedImage = colorDesc(
        ku::ImageExtentDesc::swapchainRelative());
    ku::RenderPipeline::ExternalImageBindingInfo image{
        .resourceName = "Image",
        .image = reinterpret_cast<VkImage>(1),
        .imageView = reinterpret_cast<VkImageView>(2),
        .extent = {640, 480},
        .format = expectedImage.format,
        .usage = expectedImage.usage,
        .aspect = expectedImage.aspect,
    };
    EXPECT_NO_THROW(ku::RenderPipeline::validateExternalImageBindingInfo(
        expectedImage, {640, 480}, image));
    image.usage = expectedImage.usage | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    EXPECT_NO_THROW(ku::RenderPipeline::validateExternalImageBindingInfo(
        expectedImage, {640, 480}, image));
    image.usage = 0;
    try {
        ku::RenderPipeline::validateExternalImageBindingInfo(
            expectedImage, {640, 480}, image);
        FAIL() << "zero image usage was accepted";
    } catch (const std::invalid_argument& error) {
        const std::string message(error.what());
        EXPECT_NE(message.find("Image"), std::string::npos);
        EXPECT_NE(message.find("expected="), std::string::npos);
        EXPECT_NE(message.find("actual=0"), std::string::npos);
    }
    image.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    EXPECT_THROW(ku::RenderPipeline::validateExternalImageBindingInfo(
        expectedImage, {640, 480}, image), std::invalid_argument);
    image.usage = expectedImage.usage;
    image.format = VK_FORMAT_B8G8R8A8_UNORM;
    EXPECT_THROW(ku::RenderPipeline::validateExternalImageBindingInfo(
        expectedImage, {640, 480}, image), std::invalid_argument);
    image.format = expectedImage.format;
    image.extent = {320, 240};
    EXPECT_THROW(ku::RenderPipeline::validateExternalImageBindingInfo(
        expectedImage, {640, 480}, image), std::invalid_argument);

    image.extent = {640, 480};
    image.aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
    EXPECT_THROW(ku::RenderPipeline::validateExternalImageBindingInfo(
        expectedImage, {640, 480}, image), std::invalid_argument);
    image.aspect = expectedImage.aspect;
    auto preservedImage = expectedImage;
    preservedImage.initialContent = ku::InitialContent::Preserved;
    image.contentsValid = false;
    EXPECT_THROW(ku::RenderPipeline::validateExternalImageBindingInfo(
        preservedImage, {640, 480}, image), std::invalid_argument);
    image.contentsValid = true;
    EXPECT_NO_THROW(ku::RenderPipeline::validateExternalImageBindingInfo(
        preservedImage, {640, 480}, image));

    auto expectedBuffer = bufferDesc();
    expectedBuffer.usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    ku::RenderPipeline::ExternalBufferBindingInfo buffer{
        .resourceName = "Buffer",
        .buffer = reinterpret_cast<VkBuffer>(3),
        .size = expectedBuffer.size,
        .usage = expectedBuffer.usage,
    };
    EXPECT_NO_THROW(ku::RenderPipeline::validateExternalBufferBindingInfo(
        expectedBuffer, buffer));
    buffer.usage = expectedBuffer.usage | VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    EXPECT_NO_THROW(ku::RenderPipeline::validateExternalBufferBindingInfo(
        expectedBuffer, buffer));
    buffer.usage = 0;
    try {
        ku::RenderPipeline::validateExternalBufferBindingInfo(
            expectedBuffer, buffer);
        FAIL() << "zero buffer usage was accepted";
    } catch (const std::invalid_argument& error) {
        const std::string message(error.what());
        EXPECT_NE(message.find("Buffer"), std::string::npos);
        EXPECT_NE(message.find("expected="), std::string::npos);
        EXPECT_NE(message.find("actual=0"), std::string::npos);
    }
    buffer.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    EXPECT_THROW(ku::RenderPipeline::validateExternalBufferBindingInfo(
        expectedBuffer, buffer), std::invalid_argument);
    buffer.usage = expectedBuffer.usage;
    buffer.size = expectedBuffer.size - 1;
    EXPECT_THROW(ku::RenderPipeline::validateExternalBufferBindingInfo(
        expectedBuffer, buffer), std::invalid_argument);
    buffer.size = expectedBuffer.size;
    auto preservedBuffer = expectedBuffer;
    preservedBuffer.initialContent = ku::InitialContent::Preserved;
    buffer.contentsValid = false;
    EXPECT_THROW(ku::RenderPipeline::validateExternalBufferBindingInfo(
        preservedBuffer, buffer), std::invalid_argument);
    buffer.contentsValid = true;
    EXPECT_NO_THROW(ku::RenderPipeline::validateExternalBufferBindingInfo(
        preservedBuffer, buffer));
}
