#include <gtest/gtest.h>

#include <KuEngine/RHI/RHIParameterSet.h>
#include <KuEngine/RHI/RHIPipeline.h>
#include <KuEngine/RHI/RHIShader.h>
#include <KuEngine/Render/GraphCommandContext.h>
#include <KuEngine/Render/RenderGraphCallbackPass.h>
#include <KuEngine/Render/RenderGraphResources.h>

#include <filesystem>
#include <limits>
#include <type_traits>

namespace {

VkPhysicalDeviceLimits generousLimits()
{
    VkPhysicalDeviceLimits limits{};
    limits.maxPushConstantsSize = 128;
    limits.maxBoundDescriptorSets = 4;
    limits.maxComputeWorkGroupCount[0] = 64;
    limits.maxComputeWorkGroupCount[1] = 32;
    limits.maxComputeWorkGroupCount[2] = 16;
    limits.maxDescriptorSetUniformBuffers = 8;
    limits.maxDescriptorSetStorageBuffers = 8;
    limits.maxDescriptorSetSampledImages = 8;
    limits.maxDescriptorSetSamplers = 8;
    limits.maxDescriptorSetStorageImages = 8;
    limits.maxPerStageDescriptorUniformBuffers = 8;
    limits.maxPerStageDescriptorStorageBuffers = 8;
    limits.maxPerStageDescriptorSampledImages = 8;
    limits.maxPerStageDescriptorSamplers = 8;
    limits.maxPerStageDescriptorStorageImages = 8;
    limits.maxUniformBufferRange = 256;
    limits.maxStorageBufferRange = 1024;
    limits.minUniformBufferOffsetAlignment = 16;
    limits.minStorageBufferOffsetAlignment = 8;
    return limits;
}

struct CallbackParams {
    ku::BufferHandle buffer{};
    uint32_t declarationCount = 0;
};

ku::CallbackPassDesc<CallbackParams> callbackDesc()
{
    ku::CallbackPassDesc<CallbackParams> desc{};
    desc.name = "OwnedCallback";
    desc.declare = [](ku::RenderGraphBuilder& builder, CallbackParams& params) {
        ++params.declarationCount;
        ku::BufferDesc buffer{};
        buffer.size = 64;
        buffer.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        params.buffer = builder.createBuffer("CallbackBuffer", buffer);
        builder.useBuffer(params.buffer, ku::BufferUse::TransferDestination);
    };
    desc.execute = [](
        ku::GraphCommandContext&,
        const ku::FrameData&,
        const CallbackParams&) {};
    return desc;
}

class LambdaPass final : public ku::RenderPass {
public:
    using Declare = std::function<void(ku::RenderGraphBuilder&)>;
    LambdaPass(std::string name, Declare declare)
        : m_name(std::move(name)), m_declare(std::move(declare)) {}
    [[nodiscard]] std::string_view name() const override { return m_name; }
    void setup(ku::RenderGraphBuilder& builder) override { m_declare(builder); }
private:
    std::string m_name;
    Declare m_declare;
};

} // namespace

TEST(RHIShaderContractTest, RequiresExplicitSupportedStagePathAndEntry)
{
    EXPECT_NO_THROW(ku::validateShaderDesc({
        std::filesystem::path("shader.vert.spv"),
        VK_SHADER_STAGE_VERTEX_BIT,
        "main"}));
    EXPECT_THROW(ku::validateShaderDesc({
        {}, VK_SHADER_STAGE_VERTEX_BIT, "main"}), std::invalid_argument);
    EXPECT_THROW(ku::validateShaderDesc({
        "shader.spv", VK_SHADER_STAGE_VERTEX_BIT, ""}), std::invalid_argument);
    EXPECT_THROW(ku::validateShaderDesc({
        "shader.spv", VK_SHADER_STAGE_GEOMETRY_BIT, "main"}),
        std::invalid_argument);
}

TEST(RHIPipelineContractTest, GraphicsStagesAreExactAndOrderIndependent)
{
    const VkShaderStageFlagBits exact[] = {
        VK_SHADER_STAGE_FRAGMENT_BIT, VK_SHADER_STAGE_VERTEX_BIT};
    EXPECT_NO_THROW(ku::validateGraphicsShaderStages(exact));

    const VkShaderStageFlagBits duplicate[] = {
        VK_SHADER_STAGE_VERTEX_BIT, VK_SHADER_STAGE_VERTEX_BIT};
    EXPECT_THROW(ku::validateGraphicsShaderStages(duplicate), std::invalid_argument);
    const VkShaderStageFlagBits missing[] = {VK_SHADER_STAGE_VERTEX_BIT};
    EXPECT_THROW(ku::validateGraphicsShaderStages(missing), std::invalid_argument);
    const VkShaderStageFlagBits unsupported[] = {
        VK_SHADER_STAGE_VERTEX_BIT, VK_SHADER_STAGE_COMPUTE_BIT};
    EXPECT_THROW(ku::validateGraphicsShaderStages(unsupported), std::invalid_argument);
}

TEST(RHIPipelineContractTest, PushRangesAndDispatchRespectLimits)
{
    const VkPushConstantRange valid[] = {
        {VK_SHADER_STAGE_VERTEX_BIT, 0, 16},
        {VK_SHADER_STAGE_FRAGMENT_BIT, 8, 16}};
    EXPECT_NO_THROW(ku::validatePushConstantRanges(valid, 128));
    const VkPushConstantRange overlapping[] = {
        {VK_SHADER_STAGE_COMPUTE_BIT, 0, 16},
        {VK_SHADER_STAGE_COMPUTE_BIT, 8, 16}};
    EXPECT_THROW(
        ku::validatePushConstantRanges(overlapping, 128), std::invalid_argument);
    const VkPushConstantRange unaligned[] = {
        {VK_SHADER_STAGE_COMPUTE_BIT, 2, 16}};
    EXPECT_THROW(
        ku::validatePushConstantRanges(unaligned, 128), std::invalid_argument);
    const VkPushConstantRange tooLarge[] = {
        {VK_SHADER_STAGE_COMPUTE_BIT, 120, 16}};
    EXPECT_THROW(
        ku::validatePushConstantRanges(tooLarge, 128), std::invalid_argument);

    const auto limits = generousLimits();
    EXPECT_NO_THROW(ku::validateDispatchCount(64, 32, 16, limits));
    EXPECT_THROW(ku::validateDispatchCount(0, 1, 1, limits), std::invalid_argument);
    EXPECT_THROW(ku::validateDispatchCount(65, 1, 1, limits), std::invalid_argument);
}

TEST(RHIPipelineContractTest, ComputeParameterSetMustMatchPipelineLayout)
{
    const auto layoutA = reinterpret_cast<VkDescriptorSetLayout>(uintptr_t{1});
    const auto layoutB = reinterpret_cast<VkDescriptorSetLayout>(uintptr_t{2});
    const auto set = reinterpret_cast<VkDescriptorSet>(uintptr_t{3});
    const VkDescriptorSetLayout layouts[] = {layoutA};
    EXPECT_NO_THROW(ku::validateComputeParameterSetBinding(
        layouts, 0, layoutA, set));
    EXPECT_THROW(ku::validateComputeParameterSetBinding(
        layouts, 1, layoutA, set), std::invalid_argument);
    EXPECT_THROW(ku::validateComputeParameterSetBinding(
        layouts, 0, layoutB, set), std::invalid_argument);
    EXPECT_THROW(ku::validateComputeParameterSetBinding(
        layouts, 0, layoutA, VK_NULL_HANDLE), std::invalid_argument);
}

TEST(RHIPipelineContractTest, PipelineLayoutChecksSetAndStageLimits)
{
    const auto limits = generousLimits();
    const VkPushConstantRange compute[] = {
        {VK_SHADER_STAGE_COMPUTE_BIT, 0, 16}};
    EXPECT_NO_THROW(ku::validatePipelineLayoutLimits(
        1, compute, limits, VK_SHADER_STAGE_COMPUTE_BIT));
    EXPECT_THROW(ku::validatePipelineLayoutLimits(
        5, compute, limits, VK_SHADER_STAGE_COMPUTE_BIT),
        std::invalid_argument);
    EXPECT_THROW(ku::validatePipelineLayoutLimits(
        1, compute, limits,
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT),
        std::invalid_argument);
}

TEST(RHIParameterSetContractTest, ValidatesLayoutTypesCountsRangesAndLayouts)
{
    const auto limits = generousLimits();
    const ku::ParameterBindingDesc bindings[] = {
        {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT},
        {1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT},
        {2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT},
        {3, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT}};
    EXPECT_NO_THROW(ku::validateParameterBindings(bindings, limits));
    EXPECT_THROW(ku::validateParameterBindings(
        std::span<const ku::ParameterBindingDesc>{}, limits),
        std::invalid_argument);

    const ku::ParameterBindingDesc duplicate[] = {bindings[0], bindings[0]};
    EXPECT_THROW(ku::validateParameterBindings(duplicate, limits), std::invalid_argument);
    const ku::ParameterBindingDesc unsupported[] = {
        {0, VK_DESCRIPTOR_TYPE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT}};
    EXPECT_THROW(
        ku::validateParameterBindings(unsupported, limits), std::invalid_argument);

    EXPECT_NO_THROW(ku::validateBufferParameterWrite(
        bindings[0], 256, 16, 64, limits));
    EXPECT_THROW(ku::validateBufferParameterWrite(
        bindings[0], 256, 4, 64, limits), std::invalid_argument);
    EXPECT_THROW(ku::validateBufferParameterWrite(
        bindings[1], 2048, 0, 1025, limits), std::invalid_argument);

    const auto sampler = reinterpret_cast<VkSampler>(uintptr_t{1});
    const auto view = reinterpret_cast<VkImageView>(uintptr_t{2});
    EXPECT_NO_THROW(ku::validateImageParameterWrite(
        bindings[2], sampler, view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL));
    EXPECT_THROW(ku::validateImageParameterWrite(
        bindings[2], VK_NULL_HANDLE, view,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL), std::invalid_argument);
    EXPECT_NO_THROW(ku::validateImageParameterWrite(
        bindings[3], VK_NULL_HANDLE, view, VK_IMAGE_LAYOUT_GENERAL));
    EXPECT_THROW(ku::validateImageParameterWrite(
        bindings[3], sampler, view, VK_IMAGE_LAYOUT_GENERAL),
        std::invalid_argument);
}

TEST(GraphNativeContractTest, RejectsUnsupportedCapabilitiesAndSideEffects)
{
    const ku::NativeCommandContract allowed{
        ku::NativeCommandCapability::Transfer,
        ku::NativeCommandSideEffect::ReadsDeclaredResources};
    EXPECT_NO_THROW(ku::validateNativeCommandContract(allowed, allowed, true));
    EXPECT_THROW(ku::validateNativeCommandContract(
        allowed, {}, true), std::invalid_argument);
    EXPECT_THROW(ku::validateNativeCommandContract(
        allowed,
        {ku::NativeCommandCapability::Compute,
         ku::NativeCommandSideEffect::ReadsDeclaredResources},
        true), std::invalid_argument);
    EXPECT_THROW(ku::validateNativeCommandContract(
        {ku::NativeCommandCapability::Compute,
         ku::NativeCommandSideEffect::ReadsDeclaredResources},
        {ku::NativeCommandCapability::Compute,
         ku::NativeCommandSideEffect::ReadsDeclaredResources},
        false), std::runtime_error);
    EXPECT_THROW(ku::validateNativeCommandContract(
        allowed,
        {ku::NativeCommandCapability::Transfer,
         ku::NativeCommandSideEffect::WritesDeclaredResources},
        true), std::invalid_argument);
}

TEST(GraphNativeContractTest, RejectsUnknownBitsBeforeSubsetValidation)
{
    constexpr auto unknownCapability =
        static_cast<ku::NativeCommandCapability>(1u << 3);
    constexpr auto highestCapability =
        static_cast<ku::NativeCommandCapability>(1u << 31);
    constexpr auto unknownSideEffect =
        static_cast<ku::NativeCommandSideEffect>(1u << 2);
    constexpr auto highestSideEffect =
        static_cast<ku::NativeCommandSideEffect>(1u << 31);
    const ku::NativeCommandContract transfer{
        ku::NativeCommandCapability::Transfer,
        ku::NativeCommandSideEffect::ReadsDeclaredResources};

    EXPECT_NO_THROW(ku::validateNativeCommandContractDeclaration({}));
    EXPECT_NO_THROW(ku::validateNativeCommandContractDeclaration({
        ku::NativeCommandCapability::Transfer
            | ku::NativeCommandCapability::Compute
            | ku::NativeCommandCapability::Graphics,
        ku::NativeCommandSideEffect::ReadsDeclaredResources
            | ku::NativeCommandSideEffect::WritesDeclaredResources}));
    EXPECT_NO_THROW(ku::validateNativeCommandContract(
        transfer, transfer, true));

    EXPECT_THROW(ku::validateNativeCommandContract(
        {unknownCapability, ku::NativeCommandSideEffect::None},
        {unknownCapability, ku::NativeCommandSideEffect::None}, true),
        std::invalid_argument);
    EXPECT_THROW(ku::validateNativeCommandContract(
        {unknownCapability, ku::NativeCommandSideEffect::None},
        transfer, true), std::invalid_argument);
    EXPECT_THROW(ku::validateNativeCommandContract(
        transfer,
        {unknownCapability, ku::NativeCommandSideEffect::None}, true),
        std::invalid_argument);
    EXPECT_THROW(ku::validateNativeCommandContract(
        {ku::NativeCommandCapability::Transfer, unknownSideEffect},
        {ku::NativeCommandCapability::Transfer, unknownSideEffect}, true),
        std::invalid_argument);
    EXPECT_THROW(ku::validateNativeCommandContract(
        {ku::NativeCommandCapability::Transfer, unknownSideEffect},
        transfer, true), std::invalid_argument);
    EXPECT_THROW(ku::validateNativeCommandContract(
        transfer,
        {ku::NativeCommandCapability::Transfer, unknownSideEffect}, true),
        std::invalid_argument);
    EXPECT_THROW(ku::validateNativeCommandContract(
        {ku::NativeCommandCapability::Transfer | unknownCapability,
         ku::NativeCommandSideEffect::ReadsDeclaredResources},
        transfer, true), std::invalid_argument);
    EXPECT_THROW(ku::validateNativeCommandContract(
        transfer,
        {ku::NativeCommandCapability::Transfer | unknownCapability,
         ku::NativeCommandSideEffect::ReadsDeclaredResources},
        true), std::invalid_argument);
    EXPECT_THROW(ku::validateNativeCommandContract(
        {highestCapability, highestSideEffect},
        {highestCapability, highestSideEffect}, true),
        std::invalid_argument);
}

TEST(GraphCallbackPassTest, RejectsUnknownNativeContractBeforeDeclaration)
{
    struct Params {};
    size_t declarationCalls = 0;
    const auto makeDesc = [&](ku::NativeCommandContract contract) {
        ku::CallbackPassDesc<Params> desc{};
        desc.name = "UnknownNativeContract";
        desc.nativeContract = contract;
        desc.declare = [&](ku::RenderGraphBuilder&, Params&) {
            ++declarationCalls;
        };
        desc.execute = [](
            ku::GraphCommandContext&, const ku::FrameData&, const Params&) {};
        return desc;
    };

    EXPECT_THROW(
        (ku::CallbackRenderPass<Params>{makeDesc({
            static_cast<ku::NativeCommandCapability>(1u << 31),
            ku::NativeCommandSideEffect::None})}),
        std::invalid_argument);
    EXPECT_THROW(
        (ku::CallbackRenderPass<Params>{makeDesc({
            ku::NativeCommandCapability::Transfer,
            static_cast<ku::NativeCommandSideEffect>(1u << 31)})}),
        std::invalid_argument);
    EXPECT_EQ(declarationCalls, 0u);
}

TEST(GraphCallbackPassTest, OwnsStableParametersAndRefreshesHandlesOnRecompile)
{
    ku::CallbackRenderPass<CallbackParams> pass(callbackDesc());
    static_assert(std::is_same_v<
        decltype(pass.parameters()), const CallbackParams&>);
    const auto* stableAddress = &pass.parameters();
    EXPECT_EQ(pass.executionModel(), ku::PassExecutionModel::RestrictedCallback);
    pass.setEnabled(false);
    EXPECT_FALSE(pass.enabled());
    pass.setEnabled(true);

    ku::RenderGraph first;
    auto firstBuilder = first.buildPass(first.registerPass(pass));
    pass.setup(firstBuilder);
    first.compile();
    const ku::BufferHandle firstHandle = pass.parameters().buffer;
    EXPECT_EQ(pass.parameters().declarationCount, 1u);

    ku::RenderGraph second;
    auto secondBuilder = second.buildPass(second.registerPass(pass));
    pass.setup(secondBuilder);
    second.compile();
    EXPECT_EQ(&pass.parameters(), stableAddress);
    EXPECT_EQ(pass.parameters().declarationCount, 2u);
    EXPECT_NE(pass.parameters().buffer.graphGeneration,
              firstHandle.graphGeneration);
}

TEST(RenderGraphComputeUseTest, ProducesPreciseComputeStatesAndDependencies)
{
    ku::BufferDesc buffer{};
    buffer.size = 256;
    buffer.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT
        | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
        | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT
        | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    ku::RenderGraph graph;
    LambdaPass upload("Upload", [&](ku::RenderGraphBuilder& builder) {
        builder.useBuffer(
            builder.createBuffer("Data", buffer),
            ku::BufferUse::TransferDestination);
    });
    LambdaPass computeRead("ComputeRead", [&](ku::RenderGraphBuilder& builder) {
        builder.useBuffer(
            builder.createBuffer("Data", buffer),
            ku::BufferUse::ComputeStorageRead, {32, 64});
    });
    LambdaPass computeUniform("ComputeUniform", [&](ku::RenderGraphBuilder& builder) {
        builder.useBuffer(
            builder.createBuffer("Data", buffer),
            ku::BufferUse::ComputeUniform, {0, 32});
    });
    LambdaPass computeWrite("ComputeWrite", [&](ku::RenderGraphBuilder& builder) {
        builder.useBuffer(
            builder.createBuffer("Data", buffer),
            ku::BufferUse::ComputeStorageReadWrite, {64, 64});
    });
    LambdaPass draw("Draw", [&](ku::RenderGraphBuilder& builder) {
        builder.useBuffer(
            builder.createBuffer("Data", buffer),
            ku::BufferUse::Vertex, {64, 32});
    });
    for (ku::RenderPass* pass : std::vector<ku::RenderPass*>{
             &upload, &computeRead, &computeUniform, &computeWrite, &draw}) {
        auto builder = graph.buildPass(graph.registerPass(*pass));
        pass->setup(builder);
    }
    graph.compile();

    const auto& read = graph.passes()[1].uses[0];
    EXPECT_EQ(read.state.stages, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT);
    EXPECT_EQ(read.state.access, VK_ACCESS_2_SHADER_STORAGE_READ_BIT);
    EXPECT_TRUE(read.reads);
    EXPECT_FALSE(read.writes);
    const auto& uniform = graph.passes()[2].uses[0];
    EXPECT_EQ(uniform.state.stages, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT);
    EXPECT_EQ(uniform.state.access, VK_ACCESS_2_UNIFORM_READ_BIT);
    const auto& readWrite = graph.passes()[3].uses[0];
    EXPECT_EQ(readWrite.state.access,
        VK_ACCESS_2_SHADER_STORAGE_READ_BIT
            | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT);
    EXPECT_TRUE(readWrite.reads);
    EXPECT_TRUE(readWrite.writes);
    EXPECT_GE(graph.dependencies().size(), 3u);
}

TEST(RenderGraphComputeUseTest, MapsSampledAndStorageImageUsesPrecisely)
{
    ku::ImageDesc image{};
    image.extent = ku::ImageExtentDesc::absoluteExtent(4, 4);
    image.format = VK_FORMAT_R8G8B8A8_UNORM;
    image.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
    image.aspect = VK_IMAGE_ASPECT_COLOR_BIT;
    ku::RenderGraph graph;
    LambdaPass write("Write", [&](ku::RenderGraphBuilder& builder) {
        builder.useImage(
            builder.createImage("ComputeImage", image),
            ku::ImageUse::ComputeStorageWrite);
    });
    LambdaPass sampled("Sampled", [&](ku::RenderGraphBuilder& builder) {
        builder.useImage(
            builder.createImage("ComputeImage", image),
            ku::ImageUse::ComputeSampled);
    });
    LambdaPass read("Read", [&](ku::RenderGraphBuilder& builder) {
        builder.useImage(
            builder.createImage("ComputeImage", image),
            ku::ImageUse::ComputeStorageRead);
    });
    LambdaPass readWrite("ReadWrite", [&](ku::RenderGraphBuilder& builder) {
        builder.useImage(
            builder.createImage("ComputeImage", image),
            ku::ImageUse::ComputeStorageReadWrite);
    });
    for (ku::RenderPass* pass : std::vector<ku::RenderPass*>{
             &write, &sampled, &read, &readWrite}) {
        auto builder = graph.buildPass(graph.registerPass(*pass));
        pass->setup(builder);
    }
    graph.compile();
    const auto& sampleUse = graph.passes()[1].uses[0];
    EXPECT_EQ(sampleUse.state.stages, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT);
    EXPECT_EQ(sampleUse.state.access, VK_ACCESS_2_SHADER_SAMPLED_READ_BIT);
    EXPECT_EQ(sampleUse.state.layout, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    const auto& rwUse = graph.passes()[3].uses[0];
    EXPECT_EQ(rwUse.state.access,
        VK_ACCESS_2_SHADER_STORAGE_READ_BIT
            | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT);
    EXPECT_EQ(rwUse.state.layout, VK_IMAGE_LAYOUT_GENERAL);
    EXPECT_TRUE(rwUse.reads);
    EXPECT_TRUE(rwUse.writes);
}

TEST(RenderGraphResolverRangeTest, RequiresDeclaredUseAndContainedRange)
{
    ku::PassNode pass{};
    pass.uses.push_back({
        ku::ResourceHandle{0, 7, ku::ResourceKind::Buffer}, {}, {}, {32, 64},
        ku::ImageUse::FragmentSampled, ku::BufferUse::ComputeStorageRead,
        true, false, false, SIZE_MAX});
    const auto handle = ku::ResourceHandle{0, 7, ku::ResourceKind::Buffer};
    EXPECT_TRUE(ku::passDeclaresUse(
        pass, handle, ku::BufferUse::ComputeStorageRead, {32, 64}));
    EXPECT_TRUE(ku::passDeclaresUse(
        pass, handle, ku::BufferUse::ComputeStorageRead, {48, 16}));
    EXPECT_FALSE(ku::passDeclaresUse(
        pass, handle, ku::BufferUse::ComputeStorageRead, {16, 32}));
    EXPECT_FALSE(ku::passDeclaresUse(
        pass, handle, ku::BufferUse::ComputeStorageWrite, {48, 16}));

    ku::PassNode imagePass{};
    imagePass.uses.push_back({
        ku::ResourceHandle{1, 7, ku::ResourceKind::Image}, {},
        {VK_IMAGE_ASPECT_COLOR_BIT, 1, 2, 2, 3}, {},
        ku::ImageUse::ComputeStorageRead,
        ku::BufferUse::FragmentStorageRead,
        true, false, false, SIZE_MAX});
    const auto imageHandle =
        ku::ResourceHandle{1, 7, ku::ResourceKind::Image};
    EXPECT_TRUE(ku::passDeclaresUse(
        imagePass, imageHandle, ku::ImageUse::ComputeStorageRead,
        {VK_IMAGE_ASPECT_COLOR_BIT, 2, 1, 3, 1}));
    EXPECT_FALSE(ku::passDeclaresUse(
        imagePass, imageHandle, ku::ImageUse::ComputeStorageRead,
        {VK_IMAGE_ASPECT_DEPTH_BIT, 2, 1, 3, 1}));
    EXPECT_FALSE(ku::passDeclaresUse(
        imagePass, imageHandle, ku::ImageUse::ComputeStorageRead,
        {VK_IMAGE_ASPECT_COLOR_BIT, 0, 2, 3, 1}));
}

TEST(RenderGraphComputeUseTest, RejectsComputeUseInsideAttachmentRenderingScope)
{
    ku::ImageDesc color{};
    color.extent = ku::ImageExtentDesc::absoluteExtent(4, 4);
    color.format = VK_FORMAT_R8G8B8A8_UNORM;
    color.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    color.aspect = VK_IMAGE_ASPECT_COLOR_BIT;
    ku::BufferDesc storage{};
    storage.size = 64;
    storage.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    storage.initialContent = ku::InitialContent::Preserved;
    ku::RenderGraph graph;
    LambdaPass pass("IllegalComputeAttachment", [&](ku::RenderGraphBuilder& builder) {
        builder.colorAttachment(builder.createImage("Color", color));
        builder.useBuffer(
            builder.importBuffer("Storage", storage),
            ku::BufferUse::ComputeStorageRead);
    });
    auto builder = graph.buildPass(graph.registerPass(pass));
    pass.setup(builder);
    EXPECT_THROW(graph.compile(), std::runtime_error);
}

TEST(GraphNativeContractTest, RejectsNativeComputeContractInsideAttachmentPass)
{
    struct Params { ku::ImageHandle color{}; };
    ku::CallbackPassDesc<Params> desc{};
    desc.name = "IllegalNativeComputeAttachment";
    desc.nativeContract = {
        ku::NativeCommandCapability::Compute,
        ku::NativeCommandSideEffect::None};
    desc.declare = [](ku::RenderGraphBuilder& builder, Params& params) {
        ku::ImageDesc color{};
        color.extent = ku::ImageExtentDesc::absoluteExtent(4, 4);
        color.format = VK_FORMAT_R8G8B8A8_UNORM;
        color.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        color.aspect = VK_IMAGE_ASPECT_COLOR_BIT;
        params.color = builder.createImage("Color", color);
        builder.colorAttachment(params.color);
    };
    desc.execute = [](
        ku::GraphCommandContext&, const ku::FrameData&, const Params&) {};
    ku::CallbackRenderPass<Params> pass(std::move(desc));
    ku::RenderGraph graph;
    auto builder = graph.buildPass(graph.registerPass(pass));
    pass.setup(builder);
    EXPECT_THROW(graph.compile(), std::runtime_error);
}
